#!/bin/bash
START_DIR=$(cd $(dirname "$0"); pwd)
INDRA_DIR=$START_DIR/indra
OUTPUT_DIR=$START_DIR/builds
DEPENDENCY_DIR=$START_DIR/dependencies
LOG_DIR=$START_DIR/logs

configure()
{
        echo "Configuring (for real)..."
        ./develop.py configure -G Xcode \
	        -DCMAKE_OSX_ARCHITECTURES="$1" \
	        -DOPENAL:BOOL=FALSE -DFMOD:BOOL=TRUE \
        	-DCMAKE_BUILD_TYPE=Release \
        	-DGCC_DISABLE_FATAL_WARNINGS:BOOL=TRUE \
		2>&1 > /dev/null
}

set_channel()
{
	cp -f llcommon/llversionviewer.h.in llcommon/llversionviewer.h.in.build
        if [ -n "$BUILD_RELEASE" ]; then
                sed -e s/Internal/Release/ -i '' llcommon/llversionviewer.h.in
                echo "Setting release channel..."
        else
                sed -e s/Internal/Beta/ -i '' llcommon/llversionviewer.h.in
                echo "Setting beta channel..."
        fi
}

build()
{
        echo "Building version ${VERSION_VIEWER}.${REVISION}..."
        LOGFILE=${LOG_DIR}/build_${VERSION_VIEWER}.${REVISION}
        echo "Build log is in ${LOGFILE} ."
	${INDRA_DIR}/develop.py build 2>&1 > ${LOGFILE}
}

copy_resources()
{
        echo "Copying resources..."

        if [ -n "$BUILD_RELEASE" ]; then
                SETTINGS_FILE='settings_phoenix.xml'
        else
                SETTINGS_FILE='settings_phoenixviewerbeta.xml'
        fi
        echo "--settings $SETTINGS_FILE" \
        	> Phoenix\ Viewer.app/Contents/Resources/arguments.txt
}

# Sends output file to stdout
make_disk_image()
{
        if [ -n "$BUILD_RELEASE" ]; then
                IMAGE="release-template.dmg"
                VOLUME_NAME="Phoenix Viewer ${VERSION_VIEWER}.${REVISION}"
                OUTPUT_FILE="Phoenix_Viewer_${VERSION_VIEWER}.${REVISION}_${1}.dmg"
        else
                IMAGE="beta-template.dmg"
                VOLUME_NAME="Phoenix Viewer ${VERSION_VIEWER}.${REVISION} Beta"
                OUTPUT_FILE="Phoenix_Viewer_${VERSION_VIEWER}.${REVISION}_${1}_Beta.dmg"
        fi

        # hdiutil convert won't overwrite an existing output file
        rm -f "$OUTPUT_DIR/$OUTPUT_FILE"

        # We store the templates as compressed images, so decompress it now.
        hdiutil convert "$DEPENDENCY_DIR/$IMAGE" -format UDRW \
        	-o temp-image.dmg > /dev/null
        hdiutil attach -mountpoint "$DEPENDENCY_DIR/build-image" -nobrowse \
	        temp-image.dmg > /dev/null

        # diskutil requires the full mount path.
        diskutil renameVolume "$DEPENDENCY_DIR/build-image" "$VOLUME_NAME" \
        	> /dev/null

        # Copy the viewer in
        cp -R Phoenix\ Viewer*.app "$DEPENDENCY_DIR/build-image/" > /dev/null

        # Compress/store the image and dispose of the temporary one.
        hdiutil detach "$DEPENDENCY_DIR/build-image" > /dev/null
        hdiutil convert temp-image.dmg -format UDBZ \
        	-o "$OUTPUT_DIR/$OUTPUT_FILE" > /dev/null
        rm temp-image.dmg
        echo "$OUTPUT_DIR/$OUTPUT_FILE";
}

# Sends output file to stdout
make_package()
{
        if [ -z "$BUILD_RELEASE" ]; then
                mv Phoenix\ Viewer.app Phoenix\ Viewer\ Beta.app
        fi
        echo $(make_disk_image $1)
}

upload()
{
#        echo "Uploading..."
#		#Be sure to take the upload function out before pushing any changes as it contains server credentials and hidden URLs.
        echo "The final package can be found at $1."
}

# Get viewer version from source code: sets variable VERSION_VIEWER
. ${INDRA_DIR}/Version

case "$1" in
	release)
	        echo "Building for release."
        	BUILD_RELEASE="yes"
        	;;
	beta)
	        echo "Building beta."
        	BUILD_RELEASE=""
        	;;
        *)
		echo "Usage: $0 {beta|release} [revision]"
		exit 1
		;;
esac

if [ -z "$2" ]; then
	REVISION=0
	hg update
else
	REVISION=$2
	hg update -r $REVISION
fi

if [ $? -ne 0 ]; then
        echo "Couldn't update from Mercurial."
        exit 2
fi
if [ $REVISION -eq 0 ]; then
	while read line; do
        	if [[ ${line:0:8} == "parent: " ]]; then
                	REVISION=$(echo ${line:8}|sed -e s'/:.*//')
	        fi
	done < <(hg summary)
	if [ $REVISION -eq 0 ]; then
	        echo "Could not determine Mercurial revision."
	        exit 3
	fi
fi
echo "Retrieved Mercurial revision $REVISION."

echo "Preparing..."
rm installed.xml 2>/dev/null

if [ -z $SKIP_INTEL ]; then
        echo "------------------------------------------"
        echo "                 x86 Build                "
        echo "------------------------------------------"
        echo "Cleaning..."
        cd $INDRA_DIR
        ./develop.py clean > /dev/null
        set_channel
	configure i386
        build
        if [ $? -eq 0 ]; then
                cd build-darwin-i386/newview/Release
                copy_resources

                echo "Liposuction..."
                #lipo -thin i386 Phoenix\ Viewer.app/Contents/MacOS/libhunspell-1.2.dylib -output Phoenix\ Viewer.app/Contents/MacOS/libhunspell-1.2.dylib
                lipo -thin i386 Phoenix\ Viewer.app/Contents/MacOS/libndofdev.dylib -output Phoenix\ Viewer.app/Contents/MacOS/libndofdev.dylib
                lipo -thin i386 Phoenix\ Viewer.app/Contents/MacOS/libotr.dylib -output Phoenix\ Viewer.app/Contents/MacOS/libotr.dylib
                lipo -thin i386 Phoenix\ Viewer.app/Contents/MacOS/7za -output Phoenix\ Viewer.app/Contents/MacOS/7za

                lipo -thin i386 Phoenix\ Viewer.app/Contents/Resources/SLVoice -output Phoenix\ Viewer.app/Contents/Resources/SLVoice
                lipo -thin i386 Phoenix\ Viewer.app/Contents/Resources/libortp.dylib -output Phoenix\ Viewer.app/Contents/Resources/libortp.dylib
                lipo -thin i386 Phoenix\ Viewer.app/Contents/Resources/libvivoxsdk.dylib -output Phoenix\ Viewer.app/Contents/Resources/libvivoxsdk.dylib
                #lipo -thin i386 Phoenix\ Viewer.app/Contents/Resources/llplugin/libllqtwebkit.dylib -output Phoenix\ Viewer.app/Contents/Resources/llplugin/libllqtwebkit.dylib

                echo "Packaging..."
                RESULT="$(make_package Intel)"
                upload "$RESULT"
                echo "Intel build succeeded."
        else
                echo "Intel build failed."
        fi
else
        echo "Intel build skipped."
fi

# Some cleanup.
rm installed.xml 2>/dev/null
if [ -e $INDRA_DIR/llcommon/llversionviewer.h.in.build ]; then
	mv -f $INDRA_DIR/llcommon/llversionviewer.h.in \
		$INDRA_DIR/llcommon/llversionviewer.h.in.used
	mv -f $INDRA_DIR/llcommon/llversionviewer.h.in.build \
		$INDRA_DIR/llcommon/llversionviewer.h.in
fi
echo "Finished."
