#!/bin/sh
#
# Before a release:
# * Update configure.ac and possibly ChangeLog and
#   src/sox.h (SOX_LIB_VERSION_CODE) to match release #. If this is a
#   release candidate, add "rcN" to end of version in configure.ac.
# * Update date strings and possibly copyright years in man pages.
# * Tag files to release using following form: git tag sox-14.4.0rc1
#
# Automatable release steps.   Most are optional but default to enabled.
#
# 1. Verify master git repo matches local git repo.
# 2. Build source packages.
# 3. Build Windows packages.
# 4. Generate announce email from NEWS file.
# 5. Build HTML and PDF documentation and upload to web site.
# 6. Create new release directory and upload packages to directory.
#
# After a release:
# * Make sure local tags get pushed remotely: git push --tags
# * Need to update sourceforge for recommended package to give to each
#   OS.
# * send announcement email using Mutt or similar (mutt -H sox-RELEASE.email).
# * Update front page of web site to point to latest files and give
#   latest news.
# * Modify configure.ac, increment version # and put "git" at end of #.
#

usage()
{
    cat <<HELP
Usage: `basename $0` [options] <tag_previous> <tag_current>

<tag_previous> can be special value "initial" when ran for first
time.

Options:
  --force       force overwritting an existing release
  --help        this help message
  --ignore-local-changes        don't abort on uncommitted local changes
  --no-build	Skip building executable packages
  --no-release	Skip releasing files.
  --no-short-log Put NEWS content into email instead of autogenerate.
  --remote      git remote where the change should be pushed (default "origin")
  --user <name> username on $hostname
HELP
}

build_files=yes
update_web=yes
release_files=yes
ignore_changes=no
short_log=yes
user=$USER
remote=origin
module="sox"
webpath="sourceforge.net/projects/sox/files"
rcpath=""
email_list="sox-users@lists.sourceforge.net,sox-devel@lists.sourceforge.net"
hostname="shell.sourceforge.net"
release_path="/home/frs/project/sox"
release_force="no"
web_path="/home/project-web/sox/htdocs"


while [ $# != 0 ]; do
    case "$1" in
    --force)
        release_force="yes"
        shift
        ;;
    --help)
        usage
        exit 0
        ;;
    --ignore-local-changes)
        ignore_changes=yes
        shift
        ;;
    --no-build)
	build_files=no
	shift
	;;
    --no-release)
	release_files=no
	shift
	;;
    --no-short-log)
	short_log=no
	shift
	;;
    --remote)
        shift
        remote=$1
        shift
        ;;
    --user)
	shift
	user=$1
	shift
	;;
    --*)
        echo "error: unknown option"
        usage
        exit 1
        ;;
    *)
        tag_previous="$1"
        tag_current="$2"
        shift 2
        if [ $# != 0 ]; then
            echo "error: unknown parameter"
            usage
            exit 1
        fi
        ;;
    esac
done

# configure must have been ran to get release #.
[ ! -x configure ] && autoreconf -i
[ ! -f Makefile ] && ./configure

# Check if the object has been pushed. Do do so
# 1. Check if the current branch has the object. If not, abort.
# 2. Check if the object is on $remote/branchname. If not, abort.
local_sha=`git rev-list -1 $tag_current`
current_branch=`git branch | grep "\*" | sed -e "s/\* //"`
set +e
git rev-list $current_branch | grep $local_sha > /dev/null
if [ $? -eq 1 ]; then
    echo "Cannot find tag '$tag_current' on current branch. Aborting."
    echo "Switch to the correct branch and re-run the script."
    exit 1
fi

revs=`git rev-list $remote/$current_branch..$current_branch | wc -l`
if [ $revs -ne 0 ]; then
    git rev-list $remote/$current_branch..$current_branch | grep $local_sha > /dev/null

    if [ $? -ne 1 ]; then
        echo "$remote/$current_branch doesn't have object $local_sha"
        echo "for tag '$tag_current'. Did you push branch first? Aborting."
        exit 1
    fi
fi
set -e

module="${tag_current%-*}"
if [ "x$module" = "x$tag_current" ]; then
    # release-number-only tag.
    pwd=`pwd`
    module=`basename $pwd`
    release_num="$tag_current"
else
    # module-and-release-number style tag
    release_num="${tag_current##*-}"
fi

detected_module=`grep 'PACKAGE = ' Makefile | sed 's|PACKAGE = ||'`
if [ -f $detected_module-$release_num.tar.bz2 ]; then
    module=$detected_module
fi

osx_zip="$module-${release_num}-macosx.zip"
win_zip="$module-${release_num}-win32.zip"
win_exe="$module-${release_num}-win32.exe"
src_gz="$module-${release_num}.tar.gz"
src_bz2="$module-${release_num}.tar.bz2"
release_list="$src_gz $src_bz2 $win_exe $win_zip $osx_zip"

email_file="${module}-${release_num}.email"

build()
{
    echo "Creating source packages..."
    ! make -s distcheck && echo "distcheck failed" && exit 1
    ! make -s dist-bzip2 && echo "dist-bzip2 failed" && exit 1

    if [ $update_web = "yes" ]; then
	echo "Creating HTML documentation for web site..."
	! make -s html && echo "html failed" && exit 1

	echo "Creating PDF documentation for web site..."
	! make -s pdf && echo "pdf failed" && exit 1
    fi

    echo "Creating Windows packages..."
    make -s distclean
    rm -f $win_zip
    rm -f $win_exe
    ./mingwbuild
}

MD5SUM=`which md5sum || which gmd5sum`
SHA1SUM=`which sha1sum || which gsha1sum`

create_email()
{
    cat<<EMAIL_HEADER
Subject: [ANNOUNCE] SoX ${release_num} Released
To: ${email_list}

The developers of SoX are happy to announce the release of SoX ${release_num}.
Thanks to all who contributed to this release.

EMAIL_HEADER

    if [ $short_log = "yes" ]; then
	case "$tag_previous" in
	    initial)
		range="$tag_current"
		;;
	    *)
		range="$tag_previous".."$tag_current"
		;;
	esac
	echo "git tag: $tag_current"
	echo
	git log --no-merges "$range" | git shortlog
    else
	cat NEWS
    fi

    cat<<EMAIL_FOOTER

http://$webpath/${rcpath}${module}/$release_num/$osx_zip/download

MD5:  `$MD5SUM $osx_zip`
SHA1: `$SHA1SUM $osx_zip`

http://$webpath/${rcpath}${module}/$release_num/$win_exe/download

MD5:  `$MD5SUM $win_exe`
SHA1: `$SHA1SUM $win_exe`

http://$webpath/${rcpath}${module}/$release_num/$win_zip/download

MD5:  `$MD5SUM $win_zip`
SHA1: `$SHA1SUM $win_zip`

http://$webpath/${rcpath}${module}/$release_num/$src_bz2/download

MD5:  `$MD5SUM $src_bz2`
SHA1: `$SHA1SUM $src_bz2`

http://$webpath/${rcpath}${module}/$release_num/$src_gz/download

MD5:  `$MD5SUM $src_gz`
SHA1: `$SHA1SUM $src_gz`

EMAIL_FOOTER
}

case $release_num in
    *git)
	echo "Aborting.  Should not release untracked version number."
	exit 1
	;;
    *rc?|*rc??)
	echo "Release candidate detected. Disabling web update."
	update_web=no
	rcpath="release_candidates/"
	;;
esac

if [ ! -f $osx_zip ]; then
    echo "$osx_zip files not found.  Place those in base directory and try again."
    exit 1
fi

if [ $build_files = "yes" ]; then
    build
    echo
fi

if [ ! -f $src_gz -o ! -f $src_bz2 ]; then
    echo "$src_gz or $src_bz2 not found.  Rebuild and try again"
    exit 1
fi

if [ ! -f $win_zip -o ! -f $win_exe ]; then
    echo "$win_zip or $win_exe not found.  Rebuild and try again"
    exit 1
fi

# Check for uncommitted/queued changes.
if [ "$ignore_changes" != "yes" ]; then
    set +e
    git diff --exit-code > /dev/null 2>&1
    if [ $? -ne 0 ]; then
	echo "Uncommitted changes found. Did you forget to commit? Aborting."
	echo "Use --ignore-local-changes to skip this check."
	exit 1
    fi
    set -e
fi

create_email > $email_file

username="${user},sox"

if [ $update_web = "yes" -o $release_files = "yes" ]; then
    echo "Creating shell on sourceforge for $username"
    ssh ${username}@${hostname} create
fi

if [ $update_web = "yes" ]; then
    echo "Updating web pages..."
    # Delete only PNG filenames which have random PID #'s in them.
    ssh ${username}@${hostname} rm -rf ${web_path}/soxpng
    scp -pr *.pdf *.html soxpng ${username}@${hostname}:${web_path}
    scp -p Docs.Features ${username}@${hostname}:${web_path/wiki.d}
fi

if [ $release_files = "yes" ]; then
    ssh ${username}@${hostname} mkdir -p ${release_path}/${rcpath}${module}/${release_num}
    rsync -avz --delete --progress $release_list ${username}@${hostname}:${release_path}/${rcpath}${module}/${release_num}
    # FIXME: Stop pushing this and need to find a solution to help push it later.
    #scp -p NEWS ${username}@${hostname}:${release_path}/${rcpath}${module}/${release_num}/README
fi
