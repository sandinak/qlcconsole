#!/bin/sh
#
# This script creates Q Light Controller RPM packages.

if [ ! -f platforms/linux/qlcconsole.spec ]; then
	echo ERROR: This script must be run from the top-level QLC+ source directory
	exit 1;
fi

if [ -f Makefile ]; then
	echo Cleaning objects from previous compile
	make distclean
fi

VERSION=$(head -1 debian/changelog | sed 's/.*(\(.*\)).*/\1/')
RPMBUILD=~/rpmbuild

# Prepare RPM build directory hierarchy
if [ ! -f $RPMBUILD ]; then
	echo Creating RPMBUILD hierarchy in $RPMBUILD
	mkdir -p $RPMBUILD/SOURCES $RPMBUILD/SPECS
fi

# Put a plain RPM spec file where rpmbuild expects it
sed -e "s/\$QLCPLUS_VERSION/$VERSION/g" platforms/linux/qlcconsole.spec > $RPMBUILD/SPECS/qlcconsole.spec

# Prepare a source tarball and move it under $RPMBUILD/SOURCES
echo "Packing sources into qlcconsole-$VERSION.tar.gz..."
if [ -d /tmp/qlcconsole-$VERSION ]; then
	rm -rf /tmp/qlcconsole-$VERSION
fi

mkdir /tmp/qlcconsole-$VERSION
rsync -aC . /tmp/qlcconsole-$VERSION
tar --directory=/tmp -czf /tmp/qlcconsole-$VERSION.tar.gz qlcconsole-$VERSION
mv /tmp/qlcconsole-$VERSION.tar.gz $RPMBUILD/SOURCES

cd $RPMBUILD/SPECS
rpmbuild -ba qlcconsole.spec
if [ $? == 0 ]; then
	echo Packages created in $RPMBUILD/RPMS
fi
