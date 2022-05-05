# Wreath

A multifaceted looper for the [Daisy](https://www.electro-smith.com/daisy) platform.

The concept of this looper was inspired by [Make Noise Mimeophon](https://www.makenoisemusic.com/modules/mimeophon), while the software implementation borrows a few ideas from [Monome Softcut](https://monome.org/docs/norns/softcut/). The looper was originally created for the [kxmx_bluemchen](https://kxmx-bluemchen.recursinging.com/) Eurorack module. Its first actual use is in [Repetita Versio](https://www.modwiggler.com/forum/viewtopic.php?t=261413), a firmware for [Noise Engineering's Versio platform](https://noiseengineering.us/pages/world-of-versio).

##  Disclaimer

This is my first C++ project and my first foray into embedded system programming, so the code is bound to be naive, unoptimized and, generally speaking, bad. It
kind of works, though, and I did my best to comment it for my future self and for anybody who may ask what the hell am I doing. Nevertheless, there are still
uncommented parts, and chances are that they will stay that way, unless someone is willing to fill in the blanks.

## Structure

Taking inspiration from Monome Softcut, the looper is structured like this:

- StereoLooper, the higher level class that should be directly used by your instrument. It handles two loopers, one for the left and one for the right channel, and most of its API is just a wrapper of the Looper API.

- Looper, the single looper class, used by StereoLooper. It handles the reading and the writing heads and contains the meat of the looper.

- Head, the class that represents both the reading and the writing heads.

## Usage

1) Include the "wreath/stereo_looper.h" file

```#include "wreath/stereo_looper.h"```

2) Create a StereoLooper instance

```StereoLooper looper;```

3) Init the looper by passing the sample rate and the configuration

```looper.Init(sampleRate, conf);```

4) In your AudioCallback call the Process() method (note that ```leftOut``` and ```rightOut``` are references)

```looper.Process(leftIn, rightIn, leftOut, rightOut);```

5) Once the looper has been set up, it must be started with

```looper.Start();```

## API

You should interact with the looper through the StereoLooper API. Take a look at stereo_looper.h, the methods are documented.

## License

Wreath uses the MIT license.

It can be used in both closed source and commercial projects, and does not provide a warranty of any kind.

For the full license, read the LICENSE file in the root directory.
