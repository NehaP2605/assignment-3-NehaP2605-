#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$(realpath "$1")
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
echo "OUTDIR -> ${OUTDIR}"

if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # clean the kernel build tree, removing config file with any existing configurations
    echo "Cleaning the kernel"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    #generate .config file
    echo "generating config file"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    #build the kernel
    echo "Building the kernel"
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    #build modules and devicetree
    echo "building modules"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    #build devicetree
    echo "building devicetree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

fi

echo "Adding the Image in outdir"
sudo cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p rootfs
cd rootfs
mkdir -p bin dev etc lib lib64 usr proc sys var sbin tmp home
mkdir -p usr/bin usr/sbin usr/lib
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig

else
    cd busybox
fi
echo "Make and install busybox"
# TODO: Make and install busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
    make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ./busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ./busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# Sysroot of cross compiler
SYSROOT=$(${CROSS_COMPILE}g++ -print-sysroot)
echo "Using SYSROOT = $SYSROOT"

prgmintrp=$(${CROSS_COMPILE}readelf -a ./busybox | \
	grep "program interpreter" | \
	sed -e 's/.*: \([^]]*\).*/\1/')
echo "Interpreter is : $prgmintrp"
#create destination directory in rootfs and copy
mkdir -p ${OUTDIR}/rootfs$(dirname $prgmintrp)
cp ${SYSROOT}$prgmintrp ${OUTDIR}/rootfs$prgmintrp
echo "Copied loader: ${SYSROOT}${prgmintrp} → ${OUTDIR}/rootfs${prgmintrp}"

#copy shared libraries
libs=$(${CROSS_COMPILE}readelf -a ./busybox | \
	grep "Shared library" | \
	sed -e 's/.*\[\(.*\)\].*/\1/')
mkdir -p ${OUTDIR}/rootfs/lib64
for lib in $libs; do
    # Find the library in sysroot
    libpath=$(find $SYSROOT -name "$lib" | head -n1)
    if [ -n "$libpath" ]; then
        cp "$libpath" ${OUTDIR}/rootfs/lib64/
        echo "Copied $lib to rootfs/lib64/"
    else
        echo "Warning: $lib not found in sysroot!"
    fi
done

#copying c++ runtime lib as well(c++ compatibility)
cp ${SYSROOT}/usr/lib64/libstdc++.so.6 ${OUTDIR}/rootfs/lib64/
echo "Copied ${SYSROOT}/usr/lib64/libstdc++.so.6 -> ${OUTDIR}/rootfs/lib64/"
cp ${SYSROOT}/usr/lib64/libgcc_s.so.1 ${OUTDIR}/rootfs/lib64/
echo "Copied ${SYSROOT}/usr/lib64/libgcc_s.so.1 -> ${OUTDIR}/rootfs/lib64/"

# TODO: Make device nodes
cd "$OUTDIR/rootfs/"
echo "current dir: $(pwd)"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
echo "clean and build writer utility"
cd "$FINDER_APP_DIR"
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu-
cp ./writer ${OUTDIR}/rootfs/home
echo "copied writer to ${OUTDIR}/rootfs/home"

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ./finder.sh ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/home/conf && cp ./conf/username.txt ${OUTDIR}/rootfs/home/conf
cp ./conf/assignment.txt ${OUTDIR}/rootfs/home/conf
cp ./finder-test.sh ${OUTDIR}/rootfs/home
cp ./autorun-qemu.sh ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
echo "done....................................."
