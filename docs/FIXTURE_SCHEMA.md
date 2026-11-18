# Parser Fixtures and Input Formats

## Production parser: Clipboard VST-XML 1.3 and 1.4

The production path is `VstXmlChordParser` in `src/plugin/VstXmlChordParser.*`, backed by the Expat copy shipped with the local VSTGUI SDK. It parses chord events in Clipboard VST-XML 1.3 and 1.4. The public Steinberg definition identifies 1.4 as the current DTD and lists chord `color` as an optional field.

Relevant documented shape:

```xml
<vst-xml version="1.3">
  <sourceApp>...</sourceApp>
  <chord id="..."><name>...</name>
    <projectTime domain="quarterNotes">...</projectTime>
    <pitches>...</pitches><keyNote>...</keyNote>
    <bassNote>...</bassNote><mask>...</mask>
  </chord>
</vst-xml>
```

Per the public definition, `sourceApp`, chord `id`, `keyNote`, `pitches`, and `mask` are required; `name`, `projectTime`, and `bassNote` are optional. The parser accepts only events with an absolute `projectTime`, because the timeline needs an event start. Missing time returns `MissingProjectTime`. It converts only `quarterNotes`; other domains, including the documented `seconds`, return `UnsupportedTimeDomain` with original domain and value retained for inspection.

Official examples encode `keyNote` and `bassNote` as larger integer note values. Those are retained exactly and are not treated as pitch classes. `name` is for display; raw `pitches` and `mask` remain untouched, and `quality` remains `Unknown` until a verified encoding exists.

The `vstxml-1.3-*` and `vstxml-1.4-chord.xml` fixtures exercise the public grammar and parser requirements. They are authored test inputs, not payloads captured from Cubase. Cubase Pro 15.0.30 Build 287 has been observed delivering one 1087-byte UTF-8 Text item with a VST-XML 1.4 root; the updated parser successfully converted it to four chord events at 0, 4, 6 and 7 quarter notes.

## Legacy fixture parser: `synthetic-v1`

The old `VstXmlDropAdapter::parseSyntheticV1` and its three existing fixtures are retained only for legacy regression tests. This private shape is not Clipboard VST-XML and is not invoked by the drag/drop or clipboard production paths.

```xml
<harmony-spike-fixture schema="synthetic-v1">
  <chord name="Cmaj7" keyNote="0" bassNote="0" type="Maj7"
         pitches="0,4,7,11" mask="0x891">
    <projectTime domain="quarterNotes">0</projectTime>
  </chord>
</harmony-spike-fixture>
```

It remains intentionally bounded to 1 MiB, 4096 chords, and a restricted ASCII XML subset. Its pitch class and type rules apply only to that test schema. Do not use it to infer Cubase behavior or VST-XML encodings.
