FLAC_DIR="path/to/flac" # set this variable to the "flac" directory
KO_DIR="path/to/ko" # set this variable to path of the "ko" directory

# setting environment variable required by flac test...
export FLAC__TEST_LEVEL=1
export ECHO_C=\c

for test_script in test_libFLAC.sh test_grabbag.sh test_metaflac.sh \
                    test_flac.sh test_replaygain.sh test_compression.sh \
                    test_seeking.sh test_streams.sh
do
    cd "$KO_DIR"
    bash drop_caches.sh
    sudo insmod kp.ko
    cd "$FLAC_DIR/build/test"
    echo "Running $test_script"
    LD_BIND_NOW=1 bash "../../test/$test_script"
    sudo rmmod kp
done
