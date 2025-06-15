# webmdx (Webm Decoder)

A very basic library for decoding of webm videos. Primarily made for use in [Rin](https://github.com/TareHimself/rin). See [here](./test/main.cpp) for example usage

## dependencies
- libwebm | container | required
- opus | audio | optional
- vpx | video | optional
- <s>vorbis | audio | optional</s>
- dav1d/av1 | video | optional

## issues
- naive seeking
- unreliable versioning
- little to no comments
- incomplete vorbis support
