#! /bin/sh -

while test $# -gt 0
do
    case "$1" in
        -v) VERBOSE="-vvv"
            ;;
        upload) pio run -t upload -e ratgdo_esp32dev
            ;;
        monitor) pio device monitor -e ratgdo_esp32dev
            ;;
        run) pio run -e ratgdo_esp32dev $VERBOSE
            ;;
        test) pio test -e native $VERBOSE
            ;;
        release)
            git tag $2
            ./x.sh run
            cp .pio/build/ratgdo_esp32dev/firmware.bin docs/firmware/homekit-ratgdo32-$(git describe --tag).firmware.bin
            cp .pio/build/ratgdo_esp32dev/bootloader.bin docs/firmware/homekit-ratgdo32-$(git describe --tag).bootloader.bin
            cp .pio/build/ratgdo_esp32dev/partitions.bin docs/firmware/homekit-ratgdo32-$(git describe --tag).partitions.bin
            vi docs/manifest.json
            git add docs
            git commit -m "Release $2"
            git push
            git push --tag
            ;;
        *) echo "usage: x.sh [-v] <upload|monitor|run|test>"
            exit 1
            ;;
    esac
    shift
done

exit 0
