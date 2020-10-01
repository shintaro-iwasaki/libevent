## Argobots-aware libevent

This libevent can choose Argobots (https://www.argobots.org/) for an underlying threading layer.  Its implementation is based on the version 2.0.22.

### How to compile libevent with Argobots

```
sh autogen.sh
./configure --enable-pthread-backend=argobots --with-argobots=ARGOBOTS_INSTALL_DIR --prefix=INSTALL_PATH
make -j NJOBS
make install
```
`configure` does not check availability of `libabt.so` and `abt.h`, which should be fixed in the future commit.
Please manually check if `libabt.so` is in `ARGOBOTS_INSTALL_DIR/lib` and `abt.h` in `ARGOBOTS_INSTALL_DIR/include`.

### Bug Report

Please send an email to Shintaro Iwasaki (siwasaki@anl.gov).
