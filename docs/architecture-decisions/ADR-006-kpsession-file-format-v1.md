# ADR-006: `.kpsession` v1 file-format strategy

Status: Accepted
Date: 2026-09-17

## Context

Saved sessions must retain arbitrary binary payloads, order, direction and
timing across Qt versions and platforms. The existing document save/open stubs
and text logger cannot provide this.

## Decision

Use a new binary-safe, append-only `.kpsession` format. v1 is explicitly
little-endian and consists of:

```text
8 bytes   magic: KPSN 0x1A CR LF NUL
u16       format version: 1
u16       header encoding: 1 (UTF-8 JSON)
u32       header byte length
bytes     UTF-8 JSON header
records... length-prefixed SessionEvent records
```

Each record has a fixed little-endian prefix followed by UTF-8 JSON metadata
and raw payload bytes:

```text
u32 recordLength (bytes following this field)
u16 eventType
u8  direction
u8  flags (must be zero in v1)
u64 sequence
u32 sourceId
i64 sourceTimestampNs
i64 timestampNs
u32 metadataLength
u32 payloadLength
bytes metadata JSON
bytes payload
```

Binary widths are fixed: `sequence` is `u64`, `sourceTimestampNs` and
`timestampNs` are `i64`, `eventType` is `u16`, `direction` and `flags` are `u8`,
`sourceId` is `u32`, and `recordLength`, the header byte length, `metadataLength`
and `payloadLength` are `u32`. The v1 record prefix is therefore 44 bytes.
Changing one of these widths is a format-version change, not an amendment. In
JSON, only the values that are actually 64-bit (`sequence`, the timestamps, and
any future 64-bit quantity such as an alignment mapping's reference point or
offset) are written as canonical decimal strings; 32-bit quantities stay JSON
numbers, which represent them exactly.

`metadataLength == 0` is the encoding of an empty metadata object (`{}`) and is
normal, not an error; readers must treat it as empty metadata.

The reader validates every length before allocating: header at most 16 MiB,
record at most 64 MiB, and `metadataLength + payloadLength` must exactly match
the remaining record body. Invalid magic/version/header or a malformed
complete record fails loading. A truncated final record is ignored and reported
as recovered, never treated as a valid complete event.

The JSON header contains format/version, wall-clock creation time, application
version, an array of source descriptors, an array of capture clock domains,
optional decoder hints and notes. Its two related parts are normative:

```text
sources[]                          one entry per capture source
  sourceId                         32-bit, non-zero, unique in the file
  clockDomainId                    string, references clockDomains[].id (mandatory)
  name, transport, configuration   human-readable name and the applied configuration snapshot

clockDomains[]                     one entry per clock domain used by this file
  id                               opaque string, unique in the file
  kind                             "process-monotonic" (local) or "agent-monotonic" (future remote)
  reference.sourceTimestampNs      i64, decimal string - raw source-clock value of the anchor event
  reference.sessionTimestampNs     i64, decimal string - session time of that event, exactly 0 in v1
  wallClockCorrelation             optional, informational
    wallClock                      ISO-8601 UTC string of the anchor event, best effort
    precisionNs                    i64, decimal string - uncertainty of that correlation
```

A source without a `clockDomainId` is not v1-conformant, and a clock domain
without identifier and reference pair is not v1-conformant: a timestamp that
cannot be related to a reproducible session timeline must not be written as if it
could. The wall-clock correlation is informational — the zero point of a monotonic
clock cannot in general be converted to an exact wall-clock instant, so no
exactness is claimed or required, and `sourceTimestampNs` remains authoritative.

A one-source M9 file writes an array with exactly one physical source. Each
descriptor carries its non-zero `sourceId`; its semantic role is metadata and
does not redefine the event's generic TX/RX direction. No M9 alignment mapping is
written or interpreted. A later, separately specified extension may add versioned
derived mapping metadata; it must never replace the raw per-event
`sourceTimestampNs`. The header must never contain decoded output as
authoritative data.

### v1 writer profile (normative)

The member names and value shapes a v1 writer produces are fixed here, so that no
two writers can produce two dialects of the same format:

```json
{
  "format": "komport-session",
  "version": 1,
  "created": "<ISO-8601 date and time in UTC>",
  "application": { "name": "Komport", "version": "<application version>" },
  "sources": [
    {
      "sourceId": 1,
      "clockDomainId": "local-process-monotonic-v1",
      "name": "local serial",
      "transport": "serial",
      "configuration": {
        "requested": {
          "endpoint": "<string>",
          "baudRate": "<string>",
          "dataBits": "<string>",
          "stopBits": "<string>",
          "parity": "<string>",
          "flowControl": "<string>"
        },
        "effective": {
          "endpoint": "<string>",
          "baudRate": "<string>",
          "dataBits": "<string>",
          "stopBits": "<string>",
          "parity": "<string>",
          "flowControl": "<string>"
        },
        "localBuffering": { "rxQueue": 0, "flushRate": 0 },
        "compatibility": { "startBits": "<string>" }
      }
    }
  ],
  "clockDomains": [
    {
      "id": "local-process-monotonic-v1",
      "kind": "process-monotonic",
      "reference": {
        "sourceTimestampNs": "<canonical decimal i64>",
        "sessionTimestampNs": "0"
      }
    }
  ]
}
```

- `version` is the JSON number `1` and matches the binary format version.
- `created` is an ISO-8601 UTC string; its precision is not normative, because
  `sourceTimestampNs` stays authoritative (ADR-005).
- `application.name` and `application.version` are informational strings naming
  the writing application.
- The local profile values are fixed: clock-domain id `local-process-monotonic-v1`,
  `kind` `"process-monotonic"`, source name `local serial`, `transport` `serial`.
- `configuration` carries the applied configuration snapshot: the requested
  hardware settings, the read-back effective hardware settings, the local
  buffering settings and the compatibility field. The nested members are fixed and
  exhaustive, so that two writers cannot persist two dialects:
  - `requested` and `effective` each carry exactly `endpoint`, `baudRate`,
    `dataBits`, `stopBits`, `parity` and `flowControl`, all as JSON strings.
  - `localBuffering` carries exactly `rxQueue` and `flushRate` as JSON numbers.
  - `compatibility` carries exactly `startBits` as a JSON string.
  - None of these objects carries an additional member, and a writer adds none.
  These are the field names of the live configuration metadata (SPEC-M8 §6.2) and
  of the transport's configuration type, so live events and the header agree. A
  device endpoint is part of the applied configuration and is written as it is: a
  device path is not a credential, and the prohibition on credentials in metadata
  does not require redacting it. Fields that describe a single configuration
  transaction (`applyStatus`, `changedGroups`, a message) are not part of a
  snapshot and are not written.
- 64-bit quantities are canonical decimal strings (see above), 32-bit quantities
  are JSON numbers, and `sessionTimestampNs` is the decimal string `"0"`.
- A v1 writer writes exactly these members and adds no private ones.

## Consequences

- The format has no dependency on `QDataStream` or Qt private serialization.
- Future versions can add flag-gated fields or new event types while v1 readers
  reject unsupported critical versions rather than misread them.
- CRCs, indexes, compression and simulation overlays remain later extensions.
- Sessions captured by one source remain straightforward, while a later
  multi-source recorder need not invent an incompatible second format.

## References

- `docs/komport-session-replay-simulation-architecture.md`, section 7
- ADR-002, ADR-004, ADR-005
- ADR-008
