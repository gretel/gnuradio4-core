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

The core repository now builds core-only by default. Validate it with:

```bash
cmake -S . -B build-core -DGR_USE_FETCHCONTENT_DEPS=ON -DUSE_CCACHE=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Standard blocks are moving to `gnuradio4-blocks`. Reusable non-block DSP
algorithm libraries are moving to `gnuradio4-algorithm`. The extracted
repositories should build against the installed core SDK rather than source-tree
paths from this repository.

Standard-block and algorithm-dependent tests and benchmarks that previously lived
under `core/test` and `core/benchmarks` have been removed from this core tree as
normal Git changes. They should be reintroduced in `gnuradio4-blocks`,
`gnuradio4-algorithm`, or a cross-repository integration test suite as
appropriate.

`PluginLoader` currently supports local and `file://` plugin assets in core-only
mode. HTTP(S) plugin asset loading should be reintroduced later through a
core-owned abstraction if it remains required.
