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

### options

Pass these with `-o`, for example `silo build -o shared=false -o with_av1=false`. One video codec and one audio codec have to stay on.

| Option | Default | Does |
|---|---|---|
| `shared` | `true` | Build a shared library. `false` builds a static one |
| `with_vpx` | `true` | VP8 and VP9 video, through libvpx |
| `with_av1` | `true` | AV1 video, through dav1d |
| `with_opus` | `true` | Opus audio, through libopus |
| `with_vorbis` | `true` | Vorbis audio, through libvorbis |

## tests

`test/` is its own silo project that links the packaged library and decodes a set of sample files.

```
cd test
silo sync . --release
silo run -- cmake --preset webmdx-test-release
silo run -- cmake --build --preset webmdx-test-release
silo run -- ctest --test-dir build/release --output-on-failure
```

## issues
- naive seeking
- unreliable versioning
- little to no comments
