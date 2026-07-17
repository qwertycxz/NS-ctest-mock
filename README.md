# NS-ctest-mock

This homebrew project is a minimal mock of Nintendo's connection-test server, which is used by the Nintendo Switch to verify that it can reach Nintendo's servers.

## Usage

Set up a DNS MITM to redirect connection-test requests to the loopback:

```hosts
# files under /atmosphere/hosts
127.0.0.1 *conntest.nintendowifi.net
127.0.0.1 *ctest.cdn.nintendo.net
```

If you want to block all nintendo servers, you can also just set:

```hosts
# files under /atmosphere/hosts
127.0.0.1 *nintendo*
```

Then unpack `NS-ctest-mock.tar` from the latest release to your SD card. Now you are good to go!

The sysmodule will automatically start on boot and listen for connection-test requests. You can stop and restart it with any Hekate-Toolbox-compatible sysmodule manager.

## Technical Details

After startup, it listens on `127.0.0.1:80` and returns no matter what request is made:

```http
HTTP/1.0 200 OK
Content-Length: 2
X-Organization: Nintendo

ok
```

## Build

[devkitpro/devkita64 Docker Image](https://hub.docker.com/r/devkitpro/devkita64) is suggested for building this project. Additionally, you will need to install `clang-format` if you want to run the formatting check.

```sh
# Build the project
make
# Remove the build artifacts
make clean
# Run the formatting fix
make format
# Run linting checks
make lint
# Run the formatting check
make format.check
# Run linting fixes
make lint.fix
```

Artifacts will be placed in the `atmosphere` folder.

## Maintainer

[@qwertycxz](https://github.com/qwertycxz)

## How could I contribute?

[Issues](https://github.com/qwertycxz/NS-ctest-mock/issues) and [Pull-requests](https://github.com/qwertycxz/NS-ctest-mock/pulls) are both welcomed.

## License

[Apache 2.0](LICENSE) © qwertycxz
