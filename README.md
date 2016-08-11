# gerbvQt
A simple class for drawing the libgerbv images using QPainter rather than the cairo library.

<h3>Folders and files</h3>
<ul>
  <li>gerbvQt - folder containing the class, one .h and one .cpp file</li>
  <li>example - example of usage</li>
  <li>LICENSE - GNU GPL v3 license</li>
  <li>README.md - this file</li>
</ul>

<h3>Building and running the example</h3>
<ol>
  <li>Create the "build" folder (mkdir build)</li>
  <li>Go into it (cd build)</li>
  <li>cmake ../examples/</li>
  <li>make</li>
  <li>./gerbvQtexample [any gerber file]</li>
  <li>The example will output to the build folder two files: test.png and cairo.png, which are generated using QPainter and cairo correspondingly.</li>
</ol>

<h3>Macro options</h3>
There are two macros options in gerbvQt.h: __GERBVQT_MACRO_USE_TEMPIMAGE__ and __GERBVQT_MACRO_CIRCLE_PRECISION__.<br>
See gerbvQt::drawMacroFlash(...) function for more info on these ones.<br>

<h3>References</h3>
This project uses Qt, cairo and libgerbv. Links:
<ul>
  <li>Qt: https://www.qt.io/</li>
  <li>libgerbv: http://gerbv.geda-project.org/</li>
  <li>cairo: https://cairographics.org/</li>
</ul>
