---
layout: default
title: Custom weighting curves. Open Sound Meter
---

<div class="container marketing">
    <div class="row featurette">
        <div class="col-lg-10">
            <h2 class="featurette-heading mt-md-0">Custom weighting curves</h2>
            <p class="lead">
                RTA and Magnitude plots can load custom frequency-weighting curves from plain-text files.
                Imported curves are saved with the application and can be selected from the weighting list.
            </p>

            <h3>File format</h3>
            <p>
                A weighting-curve file is a two-column text table. Each usable row defines one point:
            </p>
            <ul>
                <li>The first value is the frequency in Hz. It must be greater than zero.</li>
                <li>The second value is the gain in dB. Positive values boost; negative values cut.</li>
                <li>At least two valid points are required. Points are sorted by frequency automatically.</li>
                <li>Extra columns are ignored. A header row is skipped, and units may follow the numbers, as in <code>20 Hz, -18 dB</code>.</li>
            </ul>

            <p>
                Blank lines, rows without two numeric values, and comment lines are skipped. This means
                spreadsheet exports with a header row usually work without editing after saving as CSV or
                tab-separated text.
            </p>

            <h4>Comments</h4>
            <p>
                Lines beginning with <code>#</code> or <code>*</code> are ignored. <code>#</code> and
                <code>//</code> can also start an inline comment:
            </p>
            <pre><code>20, -18.0  # low-frequency shelf
1000, 0.0  // reference frequency</code></pre>

            <h4>Separators and decimal marks</h4>
            <table class="table">
                <thead>
                    <tr>
                        <th scope="col">Format</th>
                        <th scope="col">Example row</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>Comma-separated</td>
                        <td><code>1000, 0.0</code></td>
                    </tr>
                    <tr>
                        <td>Semicolon-separated</td>
                        <td><code>1000; 0,0</code></td>
                    </tr>
                    <tr>
                        <td>Whitespace-separated</td>
                        <td><code>1000 0.0</code></td>
                    </tr>
                </tbody>
            </table>
            <p>
                Semicolon-separated files may use a comma as the decimal mark; the application converts it
                automatically. With comma-separated or whitespace-separated columns, use a period as the
                decimal mark, since a comma is treated as a column separator.
            </p>

            <h3>Example file</h3>
            <p>
                <a href="/examples/weighting-curve.txt" download>Download weighting-curve.txt</a>.
                It is a complete, ready-to-edit curve with comments and one-third-octave points from 20 Hz
                to 20 kHz. The values are illustrative only, not a calibrated correction or a standard
                weighting curve.
            </p>
            <pre><code># Open Sound Meter custom weighting curve
# Frequency (Hz), Gain (dB)
20, -18.0
100, -5.0
1000, 0.0
10000, 0.0
20000, 0.0</code></pre>

            <h3>Importing and managing curves</h3>
            <ol>
                <li>Open the properties panel for an RTA or Magnitude plot.</li>
                <li>Click <strong>load</strong> next to the weighting list and choose a <code>.txt</code>, <code>.csv</code>, or <code>.cal</code> file.</li>
                <li>The curve name comes from the file name without its extension. If that name is already used, a number is added, for example <code>my-curve (2)</code>.</li>
                <li>Use <strong>manage</strong> to rename or remove imported curves.</li>
            </ol>

            <h3>Behavior</h3>
            <ul>
                <li>Gain is interpolated linearly against logarithmic frequency. Frequencies outside the file's range use the nearest endpoint value.</li>
                <li>Weighting is applied only in dB modes. RTA phons mode and Magnitude linear or impedance modes are unweighted.</li>
                <li>The built-in A, B, C, and Z curves are fixed and cannot be renamed or removed.</li>
            </ul>
        </div>
    </div>
</div>
