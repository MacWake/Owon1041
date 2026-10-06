# Local SCPI measurement proxy

While Owon1041 is running, a read-only SCPI endpoint listens on
`127.0.0.1:5025`. Connect the meter in Owon1041 first. The endpoint accepts
UTF-8 commands terminated by LF (`\n`) or CRLF (`\r\n`) and returns one
LF-terminated response per query. It is available to local programs only.
Each program can keep its TCP connection open and send multiple queries.

| Query | Response |
| --- | --- |
| `*IDN?` | Meter identification string. |
| `MEAS?`, `MEAS1?`, `READ?` | Fresh primary numeric reading from the meter's `MEAS1?` query, with no unit. `READ?` is a proxy alias. |
| `MEAS2?` | Fresh secondary numeric reading. |
| `MEAS:SHOW?`, `MEAS1:SHOW?`, `MEAS2:SHOW?` | Fresh display reading and unit, with the meter's non-UTF-8 unit symbols converted to UTF-8. |
| `PROX:STATE?` | JSON with connection state and the most recent display reading. No serial query is sent. |
| `PROX:READ?` | JSON with a fresh primary numeric reading, its measurement time, and the most recent display reading. |

The JSON object includes `connected`, `port`, `mode`, `mode_source`, `unit`,
`display`, and `display_at`. `PROX:READ?` also includes `value` and
`measured_at`. Times are UTC ISO 8601 strings with milliseconds. `value` is a
string so over-range or other meter responses are preserved. `mode` and `unit`
reflect the mode selected in Owon1041 (`mode_source: "app"`); if you change the
mode on the meter itself, these fields may lag behind the meter. Use a display
query when you need the unit shown by the meter.

For example:

```sh
printf 'PROX:READ?\n' | nc 127.0.0.1 5025
```

When the meter is disconnected, live queries return `ERR:DISCONNECTED`.
Meter timeouts return `ERR:TIMEOUT`; unsupported or write commands return
`ERR:UNSUPPORTED`. Queries are serialized with the app's own measurement
polling, so other programs do not open or compete for the USB serial port.
The listener allows up to eight simultaneous local connections and rejects
commands longer than 256 bytes.
