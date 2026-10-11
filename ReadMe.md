# webmdx (Webm Decoder)

A very basic library for decoding of webm videos. Primarily made for use in [Rin](https://github.com/TareHimself/rin). See [here](./test/main.cpp) for example usage


## building

[silo](https://github.com/TareHimself/silo) builds webmdx and everything it depends on from `silopkg.py`. It needs uv, git and a compiler.

```
silo build              # the library, into silo's store
silo build --debug

silo sync               # only the dependencies, then use CMake yourself
silo run -- cmake --preset webmdx-debug
silo run -- cmake --build --preset webmdx-debug
```

The recipes for its dependencies come from the registry named in `silo.config.toml`.

## issues
- naive seeking
- unreliable versioning
- little to no comments
