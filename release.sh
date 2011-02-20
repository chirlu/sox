#!/bin/sh

# Automatable release steps.  All are optional but default to enabled.
#
# 1. Build source packages.
# 2. Build Windows packages.
# 3. Generate announce email from NEWS file.
# 4. Build HTML and PDF documentation and upload to web site.
# 5. Create new release directory and upload packages to directory.
#
# After a release:
# * Need to update sourceforge for recommended package to give to each 
#   OS and send email using Mutt or similar. 
# * Update front page of web site to point to latest files and give
#   latest news.
#
# TODO: Can't be used for pre-releases right now.
# TODO: Make sure releases are tagged in cvs/git and no files remain
# uncommitted to make sure release is reproducable.

build_files=yes
update_web=yes
release_files=yes

# configure must have been ran to get release #.
[ ! -x configure ] && autoreconf -i 
[ ! -f Makefile ] && ./configure

release_num=`grep Version: sox.pc | cut -d ' ' -f 2`

osx_zip="sox-${release_num}-macosx.zip"
win_zip="sox-${release_num}-win32.zip"
win_exe="sox-${release_num}-win32.exe"
src_gz="sox-${release_num}.tar.gz"
src_bz2="sox-${release_num}.tar.bz2"
release_list="$src_gz $src_bz2 $win_exe $win_zip $osx_zip"

email_list="sox-users@lists.sourceforge.net,sox-devel@lists.sourceforge.net"
email_file="sox-${release_num}.email"

username="${USER},sox"
hostname="shell.sourceforge.net"
release_path="/home/frs/project/s/so/sox/sox"
release_force="no"
web_path="/home/project-web/sox/htdocs"

build()
{
    echo "Creating source packages..."
    ! make -s distcheck && echo "distcheck failed" && exit 1
    ! make -s dist-bzip2 && echo "dist-bzip2 failed" && exit 1

    echo "Creating Windows packages..."
    make -s distclean
    rm -f $win_zip
    rm -f $win_exe
    ./mingwbuild 

    if [ $update_web = "yes" ]; then
	echo "Creating HTML documentation for web site..."
	! make -s html && echo "html failed" && exit 1

	echo "Creating PDF documentation for web site..."
	! make -s pdf && echo "pdf failed" && exit 1
    fi
}

create_email()
{
    cat<<EMAIL_HEADER
Subject: [ANNOUNCE] SoX ${release_num} Released
To: ${email_list}

EMAIL_HEADER

cat NEWS
}

case $release_num in
    *cvs|*cgit)
	echo "Aborting.  Should not release untracked version number."
	exit 1
	;;
    *rc*)
	echo "TODO: Upload path for RC's is different.  Aborting."
	exit 1;
	;;
esac

if [ ! -f $osx_zip ]; then
    echo "$osx_zip files not found.  Place those in base directory and try again."
    exit 1
fi


if [ $build_files = "yes" ]; then
    build
fi

if [ ! -f $src_gz -o ! -f $src_bz2 ]; then
    echo "$src_gz or $src_bz2 not found.  Rebuild and try again"
    exit 1
fi

if [ ! -f $win_zip -o ! -f $win_exe ]; then
    echo "$win_zip or $win_exe not found.  Rebuild and try again"
    exit 1
fi

create_email > $email_file

if [ $update_web = "yes" -o $release_files = "yes" ]; then
    echo "Creating shell on sourceforge for $username"
    ssh ${username}@${hostname}  create
    sleep 30
fi

if [ $update_web = "yes" ]; then
    echo "Updating web pages..."
    # Delete only PNG filenames which have random PID #'s in them.
    ssh ${username}@${hostname} rm -rf ${web_path}/soxpng
    scp -pr *.pdf *.html soxpng ${username}@${hostname}:${web_path}
    scp -p Docs.Features ${username}@${hostname}:${web_path/wiki.d}
fi

if [ $release_files = "yes" ]; then
    echo "Checking for an existing release..."
    if ssh ${username}@${hostname} ls ${release_path}/${release_num}/$src_gz >/dev/null 2>&1; then
    if [ "$release_force" != "yes" ]; then
	echo "error: file already exists!"
	exit 1
    fi
    ssh ${username}@${hostname} mkdir -p ${release_path}/${release_num}
    scp -p $release_list ${username}@${hostname}:${release_path}/${release_num}
    scp -p NEWS ${username}@${hostname}:${release_path}/${release_num}/README.txt
fi
