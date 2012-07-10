/* libSoX device driver: MS-Windows audio   (c) 2009 SoX contributors
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>

/* Larger means more latency (difference between the status line and the audio you hear),
 * but it means lower chance of stuttering/glitching. 2 buffers is usually enough. Use
 * 4 if you want to be extra-safe.
 */
#define num_buffers 4

typedef struct waveaudio_priv_t
{
  /* Handle to the input device (microphone, line in, etc.). NULL if playing. */
  HWAVEIN hin;

  /* Handle to the output device (speakers, line out, etc.). NULL if recording. */
  HWAVEOUT hout;

  /* Event that becomes signaled when a the system has finished processing a buffer. */
  HANDLE block_finished_event;

  /* Data transfer buffers. The lpData member of the first buffer points at
   * data[buf_len*sample_size*0], the second buffer's lpData points
   * data[buf_len*sample_size*1], etc. The dwUser field contains the number
   * of samples of this buffer that have already been processed.
   */
  WAVEHDR headers[num_buffers];

  /* The combined data area shared by all transfer buffers. */
  char * data;

  /* The number of samples that can fit into one transfer buffer. */
  size_t buf_len;

  /* Index of the buffer that we're currently processing. For playback, this is the buffer
   * that will receive the next samples. For recording, this is the buffer from which we'll
   * be getting the next samples. If no buffers are ready for processing, this is the buffer
   * that will be the next to become ready.
   */
  unsigned current;

  /* Width of a sample in bytes: 1, 2, 3, or 4. */
  unsigned sample_width;

  /* If there has been an error, this has the Win32 error code. Otherwise, this is 0. */
  DWORD error;
} priv_t;

static void fail(sox_format_t* ft, DWORD code, const char* context)
{
  char message[256];
  DWORD formatMessageOk = FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    code,
    0,
    message,
    sizeof(message) / sizeof(message[0]),
    NULL);
  if (formatMessageOk)
    lsx_fail_errno(ft, SOX_EOF, "WaveAudio %s failed with code %d: %s", context, (int)code, message);
  else
    lsx_fail_errno(ft, SOX_EOF, "WaveAudio %s failed with unrecognized MMSYSERR code %d.", context, (int)code);
}

static int stop(sox_format_t* ft)
{
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return SOX_EOF;

  if (priv->hin)
  {
    priv->error = waveInReset(priv->hin);
    priv->error = waveInClose(priv->hin);
  }
  
  if (priv->hout && !priv->error)
  {
    while ((priv->error = waveOutClose(priv->hout)) == WAVERR_STILLPLAYING)
    {
      WaitForSingleObject(priv->block_finished_event, INFINITE);
    }
  }
  else if (priv->hout)
  {
    priv->error = waveOutReset(priv->hout);
    priv->error = waveOutClose(priv->hout);
  }

  if (priv->block_finished_event)
    CloseHandle(priv->block_finished_event);

  if (priv->data)
    free(priv->data);

  return SOX_SUCCESS;
}

static int check_format(
    WAVEFORMATEXTENSIBLE* pfmt,
    int recording,
    unsigned dev,
    unsigned channels,
    unsigned width,
    unsigned hertz)
{
  static unsigned char const SubformatPcm[] = "\x01\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71";
  const unsigned bytewidth = width > 24 ? 4 : width > 16 ? 3 : width > 8 ? 2 : 1;
  const int extend = channels > 2 || bytewidth > 2;
  DWORD error;
  pfmt->Format.wFormatTag = extend ? WAVE_FORMAT_EXTENSIBLE : WAVE_FORMAT_PCM;
  pfmt->Format.nChannels = channels;
  pfmt->Format.nSamplesPerSec = hertz;
  pfmt->Format.nAvgBytesPerSec = channels * bytewidth * hertz;
  pfmt->Format.nBlockAlign = channels * bytewidth;
  pfmt->Format.wBitsPerSample = bytewidth * 8;
  pfmt->Format.cbSize = extend ? 22 : 0;
  pfmt->Samples.wValidBitsPerSample = width;
  pfmt->dwChannelMask = 0;
  memcpy(&pfmt->SubFormat, SubformatPcm, 16);
  if (recording)
    error = waveInOpen(0, dev, &pfmt->Format, 0, 0, WAVE_FORMAT_QUERY);
  else
    error = waveOutOpen(0, dev, &pfmt->Format, 0, 0, WAVE_FORMAT_QUERY);
  return error == MMSYSERR_NOERROR;
}

static int negotiate_format(sox_format_t* ft, WAVEFORMATEXTENSIBLE* pfmt, unsigned dev)
{
  int recording = ft->mode == 'r';

  unsigned precision = ft->encoding.bits_per_sample;
  if (precision > 32)
    precision = 32;
  else if (precision < 8)
    precision = 8;

  while (precision > 0)
  {
    if (check_format(pfmt, recording, dev, ft->signal.channels, precision, (unsigned)ft->signal.rate))
      return 1;
    precision = (precision - 1) & ~0x7;
  }

  return 0;
}

static int start(sox_format_t* ft)
{
  size_t i;
  UINT dev;
  WAVEFORMATEXTENSIBLE fmt;
  int recording = ft->mode == 'r';
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return SOX_EOF;
  memset(&fmt, 0, sizeof(fmt));

  /* Handle AUDIODEV device selection:
   * NULL, blank, or "default" gets you the default device (WAVE_MAPPER = -1).
   * An integer value gets you the device with that device number, if it exists (counting starts at 0).
   * A string gets you the device with that name, if it exists.
   */
  if (ft->filename == 0 || ft->filename[0] == 0 || !strcasecmp("default", ft->filename))
  {
    dev = WAVE_MAPPER;
  }
  else
  {
    WAVEINCAPSA incaps;
    WAVEOUTCAPSA outcaps;
    const char *dev_name;
    char* dev_num_end;
    dev = strtoul(ft->filename, &dev_num_end, 0);
    if (dev_num_end[0] == 0)
    {
      if (recording)
        priv->error = waveInGetDevCapsA(dev, &incaps, sizeof(incaps));
      else
        priv->error = waveOutGetDevCapsA(dev, &outcaps, sizeof(outcaps));

      if (priv->error)
      {
        lsx_fail_errno(ft, ENODEV, "WaveAudio was unable to find the AUDIODEV %s device \"%s\".", recording ? "input" : "output", ft->filename);
        return SOX_EOF;
      }
    }
    else
    {
      UINT dev_count = recording ? waveInGetNumDevs() : waveOutGetNumDevs();
      for (dev = (UINT)-1; dev == WAVE_MAPPER || dev < dev_count; dev++)
      {
        if (recording)
        {
          priv->error = waveInGetDevCapsA(dev, &incaps, sizeof(incaps));
          dev_name = incaps.szPname;
        }
        else
        {
          priv->error = waveOutGetDevCapsA(dev, &outcaps, sizeof(outcaps));
          dev_name = outcaps.szPname;
        }

        if (priv->error)
        {
          fail(ft, priv->error, recording ? "waveInGetDevCapsA" : "waveOutGetDevCapsA");
          return SOX_EOF;
        }

        if (!strncasecmp(ft->filename, dev_name, 31))
          break;
      }

      if (dev == dev_count)
      {
        lsx_fail_errno(ft, ENODEV, "WaveAudio was unable to find the AUDIODEV %s device \"%s\".", recording ? "input" : "output", ft->filename);
        return SOX_EOF;
      }
    }
  }

  if (!negotiate_format(ft, &fmt, dev))
  {
    lsx_fail_errno(ft, ENODEV, "WaveAudio was unable to negotiate a sample format.");
    return SOX_EOF;
  }

  priv->sample_width = fmt.Format.wBitsPerSample / 8;
  ft->signal.precision = fmt.Samples.wValidBitsPerSample;
  ft->signal.channels = fmt.Format.nChannels;
  lsx_report(
      "WaveAudio negotiated %s device %d with %uHz %uCh %uprec %uwidth.",
      recording ? "input" : "output",
      (int)dev,
      (unsigned)fmt.Format.nSamplesPerSec,
      (unsigned)fmt.Format.nChannels,
      (unsigned)fmt.Samples.wValidBitsPerSample,
      (unsigned)fmt.Format.wBitsPerSample);

  priv->buf_len = sox_globals.bufsiz;
  priv->data = lsx_malloc(priv->buf_len * priv->sample_width * num_buffers);
  if (!priv->data)
  {
    lsx_fail_errno(ft, SOX_ENOMEM, "Out of memory.");
    return SOX_EOF;
  }

  priv->block_finished_event = CreateEventA(NULL, FALSE, FALSE, NULL);
  if (!priv->block_finished_event)
  {
    priv->error = GetLastError();
    fail(ft, priv->error, "CreateEventA");
    stop(ft);
    return SOX_EOF;
  }

  if (recording)
    priv->error = waveInOpen(&priv->hin, dev, &fmt.Format, (DWORD_PTR)priv->block_finished_event, 0, CALLBACK_EVENT);
  else
    priv->error = waveOutOpen(&priv->hout, dev, &fmt.Format, (DWORD_PTR)priv->block_finished_event, 0, CALLBACK_EVENT);

  if (priv->error != MMSYSERR_NOERROR)
  {
    fail(ft, priv->error, recording ? "waveInOpen" : "waveOutOpen");
    stop(ft);
    return SOX_EOF;
  }

  for (i = 0; i < num_buffers; i++)
  {
    priv->headers[i].lpData = priv->data + priv->buf_len * priv->sample_width * i;
    priv->headers[i].dwBufferLength = priv->buf_len * priv->sample_width;

    if (recording)
      priv->error = waveInPrepareHeader(priv->hin, &priv->headers[i], sizeof(priv->headers[i]));
    else
      priv->error = waveOutPrepareHeader(priv->hout, &priv->headers[i], sizeof(priv->headers[i]));

    if (priv->error != MMSYSERR_NOERROR)
    {
      fail(ft, priv->error, recording ? "waveInPrepareHeader" : "waveOutPrepareHeader");
      stop(ft);
      return SOX_EOF;
    }

    if (recording)
    {
      priv->error = waveInAddBuffer(priv->hin, &priv->headers[i], sizeof(priv->headers[i]));
      if (priv->error != MMSYSERR_NOERROR)
      {
        fail(ft, priv->error, "waveInAddBuffer");
        stop(ft);
        return SOX_EOF;
      }
    }
  }

  if (recording)
  {
    priv->error = waveInStart(priv->hin);
    if (priv->error != MMSYSERR_NOERROR)
    {
      fail(ft, priv->error, "waveInStart");
      stop(ft);
      return SOX_EOF;
    }
  }

  return SOX_SUCCESS;
}

static size_t waveread(sox_format_t * ft, sox_sample_t* buf, size_t len)
{
  size_t copied = 0;
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return (size_t)SOX_EOF;

  while (!priv->error && copied < len)
  {
    LPWAVEHDR header = &priv->headers[priv->current];
    if (0 == (header->dwFlags & WHDR_INQUEUE) ||
      0 != (header->dwFlags & WHDR_DONE))
    {
      size_t length = header->dwBytesRecorded / priv->sample_width;
      size_t ready = min(len - copied, length - header->dwUser);
      size_t i;

      switch (priv->sample_width)
      {
      case 1:
          for (i = 0; i < ready; ++i)
          {
            buf[copied++] = SOX_UNSIGNED_8BIT_TO_SAMPLE(((uint8_t *)header->lpData)[header->dwUser++], dummy);
          }
          break;
      case 2:
          for (i = 0; i < ready; ++i)
          {
            buf[copied++] = SOX_SIGNED_16BIT_TO_SAMPLE(((int16_t *)header->lpData)[header->dwUser++], dummy);
          }
          break;
      case 3:
          for (i = 0; i < ready; ++i)
          {
            sox_int24_t x = *(UNALIGNED sox_int24_t*)(header->lpData + header->dwUser * 3);
            buf[copied++] = SOX_SIGNED_24BIT_TO_SAMPLE(x, dummy);
            header->dwUser++;
          }
          break;
      case 4:
          for (i = 0; i < ready; ++i)
          {
            buf[copied++] = SOX_SIGNED_32BIT_TO_SAMPLE(((int32_t *)header->lpData)[header->dwUser++], dummy);
          }
          break;
      }

      if (header->dwUser == length)
      {
        priv->error = waveInAddBuffer(priv->hin, header, sizeof(*header));
        priv->current = (priv->current + 1) % num_buffers;
        priv->headers[priv->current].dwUser = 0;
        if (priv->error)
        {
          fail(ft, priv->error, "waveInAddBuffer");
          copied = 0;
        }
      }
    }
    else
    {
      WaitForSingleObject(priv->block_finished_event, INFINITE);
    }
  }

  return copied;
}

static size_t wavewrite(sox_format_t * ft, const sox_sample_t* buf, size_t len)
{
  unsigned clips = 0;
  size_t copied = 0;
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return (size_t)SOX_EOF;

  while (!priv->error && copied < len)
  {
    LPWAVEHDR header = &priv->headers[priv->current];
    if (0 == (header->dwFlags & WHDR_INQUEUE) ||
      0 != (header->dwFlags & WHDR_DONE))
    {
      size_t ready = min(len - copied, priv->buf_len - header->dwUser);
      size_t i;

      switch (priv->sample_width)
      {
      case 1:
          for (i = 0; i < ready; ++i)
          {
            SOX_SAMPLE_LOCALS;
            ((uint8_t *)header->lpData)[header->dwUser++] = SOX_SAMPLE_TO_UNSIGNED_8BIT(buf[copied++], clips);
          }
          break;
      case 2:
          for (i = 0; i < ready; ++i)
          {
            SOX_SAMPLE_LOCALS;
            ((int16_t *)header->lpData)[header->dwUser++] = SOX_SAMPLE_TO_SIGNED_16BIT(buf[copied++], clips);
          }
          break;
      case 3:
          for (i = 0; i < ready; ++i)
          {
            SOX_SAMPLE_LOCALS;
            unsigned char* pdata = (unsigned char*)header->lpData + header->dwUser * 3;
            sox_int24_t x = SOX_SAMPLE_TO_SIGNED_24BIT(buf[copied++], clips);
            *pdata++ = x;
            *pdata++ = x >> 8;
            *pdata++ = x >> 16;
            header->dwUser++;
          }
          break;
      case 4:
          for (i = 0; i < ready; ++i)
          {
            ((int32_t *)header->lpData)[header->dwUser++] = SOX_SAMPLE_TO_SIGNED_32BIT(buf[copied++], clips);
          }
          break;
      }

      header->dwBufferLength = header->dwUser * priv->sample_width;
      priv->error = waveOutWrite(priv->hout, header, sizeof(*header));
      priv->current = (priv->current + 1) % num_buffers;
      priv->headers[priv->current].dwUser = 0;

      if (priv->error)
      {
        fail(ft, priv->error, "waveOutWrite");
        copied = 0;
      }
    }
    else
    {
      WaitForSingleObject(priv->block_finished_event, INFINITE);
    }
  }

  return copied;
}

LSX_FORMAT_HANDLER(waveaudio)
{
  static const char * const names[] = {"waveaudio", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 24, 32, 8, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
  "Windows Multimedia Audio", names, 
  SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
  start, waveread, stop,
  start, wavewrite, stop,
  NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
