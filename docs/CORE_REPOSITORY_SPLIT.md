# Core Repository Split Preparation

GNU Radio 4 is being prepared so the current monorepo can become `gnuradio4-core`
while preserving its full Git history. The reusable algorithm library and standard
MIT block libraries are expected to become external repositories that build
against the installed core SDK.

During this transition:

- core code and public core headers must not depend on `algorithm/` or `blocks/`;
- algorithm code may depend on the installed core SDK;
- standard block libraries may depend on the installed core SDK and algorithm package;
- external blocklib repositories should use the installed `gnuradio4` and
  `GnuRadioBlockLib` CMake packages, including the installed
  `gnuradio_4_0_parse_registrations` tool.

For local validation, the in-tree algorithm and standard block libraries can be
disabled with:

```bash
cmake -S . -B build-core-only \
  -DGR4_ENABLE_ALGORITHM=OFF \
  -DGR4_ENABLE_BLOCKS=OFF
```

The default build still includes the in-tree algorithm and standard blocks until
the follow-on split commit removes those directories from `gnuradio4-core`.
