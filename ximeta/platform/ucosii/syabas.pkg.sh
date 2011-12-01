export nxp-cpu=arm
export nxp-os=ucosii
export nxp-vendor=syabas

NDAS_SRC_DIR="../.."

PKGDIR=SYABAS_PKG
SDKDIR=$NDAS_SRC_DIR/../Syabas3_Buffalo_sdk
SUPDIR=syabas_sup

SDK_MODIFIED_SRC="Makefile.sh main.c tcpip/sdesigns/enet.c"
SDK_MODIFIED_DIR="tcpip/sdesigns"

rm -rf $PKGDIR
mkdir -p $PKGDIR
mkdir -p $PKGDIR/ndas/inc/ndasuser
mkdir -p $PKGDIR/ndas/inc/sal
mkdir -p $PKGDIR/ndas/inc/sal/ucosii
mkdir -p $PKGDIR/ndas/sal
mkdir -p $PKGDIR/sdk

cp $SUPDIR/Makefile $PKGDIR/ndas
cp $SUPDIR/readme.txt $PKGDIR/
cp $SUPDIR/*.c $PKGDIR/ndas
#cp $SUPDIR/version.txt $PKGDIR/
#cp $SUPDIR/doc/ndasdoc.chm $PKGDIR/
cp $NDAS_SRC_DIR/inc/ndasuser/def.h $PKGDIR/ndas/inc/ndasuser
cp $NDAS_SRC_DIR/inc/ndasuser/info.h $PKGDIR/ndas/inc/ndasuser
cp $NDAS_SRC_DIR/inc/ndasuser/ndaserr.h $PKGDIR/ndas/inc/ndasuser
cp $NDAS_SRC_DIR/inc/ndasuser/ndasuser.h $PKGDIR/ndas/inc/ndasuser
cp $NDAS_SRC_DIR/inc/ndasuser/io.h $PKGDIR/ndas/inc/ndasuser
cp $NDAS_SRC_DIR/inc/sal/*.h $PKGDIR/ndas/inc/sal
cp $NDAS_SRC_DIR/inc/sal/ucosii/*.h $PKGDIR/ndas/inc/sal/ucosii
cp $NDAS_SRC_DIR/platform/ucosii/sal/*.c $PKGDIR/ndas/sal


make -C $NDAS_SRC_DIR ucos-clean
make -C $NDAS_SRC_DIR ucos-release
cp $NDAS_SRC_DIR/../xplat-release/arm_ucosii/libndas.a $PKGDIR/ndas/libndas-release.a

make -C $NDAS_SRC_DIR ucos-clean
make -C $NDAS_SRC_DIR ucos-debug
cp $NDAS_SRC_DIR/../xplat-release/arm_ucosii/libndas.a $PKGDIR/ndas/libndas-debug.a

for i in $SDK_MODIFIED_DIR
do
mkdir -p $PKGDIR/sdk/$i
done

for i in $SDK_MODIFIED_SRC
do
cp $SUPDIR/sdk/$i $PKGDIR/sdk/$i
done

mkdir -p $SDKDIR/ndas
cp -rf $PKGDIR/ndas/* $SDKDIR/ndas/

sleep 2
tar czf arm.ucosii.ndas.tgz $PKGDIR

