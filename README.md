# Sox extended

This is a patched version of [sox](https://github.com/chirlu/sox) merged with [this extension](https://github.com/jdesbonnet/joe-desbonnet-blog/tree/master/projects/sox-log-spectrogram) by [jdesbonnet](https://github.com/jdesbonnet).

## Compiling

Compiling the source requires a C compiler, autoconf, autoconf-archive.

To install the dependencies on Debian based systems:

```bash
sudo apt-get install build-essential autoconf autoconf-archive
```

To compile the source:

```bash
chmod +x compile.sh
./compile.sh
```

## License

This project is licensed under the GPL v2 license. See the [LICENSE](LICENSE.GPL) file for details.
