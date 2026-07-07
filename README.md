# NS-ctest-server

Minimal libnx C sysmodule for serving Nintendo Switch connection-test HTTP
responses on local loopback.

It listens on `127.0.0.1:80` and returns:

```http
HTTP/1.0 200 OK
Content-Length: 2
X-Organization: Nintendo

ok
```

## Build

```sh
make
```

The packaged boot2 sysmodule is written to:

```text
out/4200000000004354/
```

`ctest.cdn.nintendo.net` still needs to be DNS MITM'd to `127.0.0.1`;
this module does not alter DNS.
