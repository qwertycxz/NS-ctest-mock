# NS-ctest-server

Minimal Atmosphere sysmodule for serving Nintendo Switch connection-test HTTP
responses locally.

It listens on TCP port 80 and returns:

```http
HTTP/1.1 200 OK
Content-Length: 2
Content-Type: text/plain
Connection: close

ok
```

## Config

Runtime config path:

```text
sdmc:/config/NS-ctest-server/config.json
```

Schema:

```json
{
  "listen_ip": "0.0.0.0"
}
```

If the config is missing or invalid, the module falls back to `0.0.0.0`.

## Build

```sh
git submodule update --init --recursive
make
```

The packaged boot2 sysmodule is written to:

```text
out/4200000000004354/
```

`ctest.cdn.nintendo.net` still needs to be DNS MITM'd to the Switch/local
listener address; this module does not alter DNS.
