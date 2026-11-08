# Open Sound Meter
Crossplatform measurement software for tuning sound systems.

**Supported systems:** macOS, Windows, Linux

Current version: v1.6.1

[Project page](https://opensoundmeter.com/)

![](https://private-user-images.githubusercontent.com/683461/391698445-324038eb-a325-4951-9c29-ee404bebbf19.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MzkxMzYyMTcsIm5iZiI6MTczOTEzNTkxNywicGF0aCI6Ii82ODM0NjEvMzkxNjk4NDQ1LTMyNDAzOGViLWEzMjUtNDk1MS05YzI5LWVlNDA0YmViYmYxOS5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjUwMjA5JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI1MDIwOVQyMTE4MzdaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT1mODdmNmVmYmI0NTEyM2Y3YTg2MjZiMzc2MzE4NmM0M2MwOTdkYzc4ZTdlNDRlMWZkOGVjYTZkZWM1M2Y3NDczJlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCJ9.w1SSK4YF7kJ7cd3dIp8xrYIv-o0dCXNddjCdjO2vxYk)

Qt5.15 C++17

## Custom weighting curves

RTA and Magnitude plots can import custom weighting curves from `.txt`, `.csv`, or `.cal` files. See the [file-format guide](docs/weighting-curves.md) and the [ready-to-edit example](docs/examples/weighting-curve.txt).


## Long impulse responses

Measurement and windowing modes now include FFT 17 through FFT 20, up to 1,048,576 samples per transform. The deconvolution buffer retains the full causal half of the record, so long room decays can be captured without truncation: FFT 19 at 96 kHz and FFT 20 at 192 kHz both retain about 2.7 seconds of impulse response. The complete record is used by the impulse-response plots and by speech-intelligibility analysis.

## Speech intelligibility

The Speech Intelligibility chart calculates STI, STIPA and RASTI from an impulse response using the indirect method of IEC 60268-16. Optional **Noise & masking** corrections apply the standard's auditory-masking and absolute speech-reception-threshold terms, and can also include ambient noise. Enter seven octave-band speech levels and, when needed, seven noise levels from 125 Hz to 8 kHz in dB SPL; the status line reports whether corrections are off, applied, or why the entered levels cannot be used. Correction settings are stored with the chart.

### Worked correction example

Select a stored impulse-response measurement, open the Speech Intelligibility chart, and enable **Noise & masking**. Enter the speech levels for the speech signal alone, without the background noise, in the order 125, 250, 500, 1k, 2k, 4k and 8k Hz, separated by spaces or commas. For example:

`62, 65, 68, 70, 71, 69, 66`

If background noise is present, enter the seven matching octave-band noise levels in the **Noise dB SPL** field. For example:

`48, 44, 40, 36, 33, 30, 28`

Leave **Noise dB SPL** empty to apply only auditory masking and the absolute reception threshold. The chart recalculates automatically when a field loses focus or the checkbox changes.

The status line below the chart reports which correction was used, or what needs fixing. These are the relevant parts of that line:

| Status line contains | Meaning |
|---|---|
| `noise/masking corrections off` | The checkbox is off; the displayed values use the unmodified impulse response. |
| `masking/threshold correction applied` | Seven valid speech levels were accepted; masking and the absolute reception threshold were applied. |
| `noise + masking corrections applied` | Valid speech and noise levels were accepted; the ambient-noise term was applied as well. |
| `enter seven speech octave-band levels in dB SPL` | The checkbox is on but the speech field is empty; the chart remains uncorrected. |
| `speech levels: expected seven values for 125 Hz to 8 kHz` | The speech field does not contain exactly seven numbers; correct the field. |
| `speech levels: invalid number: <value>` | One speech value is not numeric; correct the field. |
| `noise levels: expected seven values for 125 Hz to 8 kHz` | The noise field does not contain exactly seven numbers; noise is left out, and masking stays active only if the speech levels are valid. |
| `noise levels: invalid number: <value>` | One noise value is not numeric; noise is left out, and masking stays active only if the speech levels are valid. |

Malformed or missing levels do not stop the chart; they leave the affected correction out and show the reason in the status line. If the line includes `IR ... (under 1.6 s)`, the stored impulse response is shorter than 1.6 s and the status is flagged in orange; capture a longer response to remove the warning.

## Tests

QtTest targets cover magnitude averaging, weighting curves, speech intelligibility (including IEC 60268-16 correction math), long impulse-response deconvolution, and chart settings. The [Qt tests workflow](.github/workflows/qt-averaging.yml) runs the suites on Linux, with the weighting, speech, long-impulse and chart suites also running on Windows and macOS.

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.
