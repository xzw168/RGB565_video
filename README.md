# RGB565_video

Simple example for playing RGB565 raw video

## Convert video for SPIFFS

#### 220x124@12fps

`ffmpeg -t 2 -i input.mp4 -vf "fps=15,scale=220:-1" -c:v rawvideo -pix_fmt rgb565be output.rgb`

## Convert audio + video for SD card

### audio

#### 44.1 kHz

`ffmpeg -i input.mp4 -f u16be -acodec pcm_u16le -ar 44100 -ac 1 -af "volume=0.5" 44100_u16le.pcm`

#### MP3

`ffmpeg -i input.mp4 -ac 1 -q:a 9 44100.mp3`

### video

#### 220x176@7fps

`ffmpeg -i input.mp4 -vf "fps=7,scale=-1:176,crop=220:in_h:(in_w-220)/2:0" -c:v rawvideo -pix_fmt rgb565le 220_7fps.rgb`

#### 220x176@9fps

`ffmpeg -i input.mp4 -vf "fps=9,scale=-1:176,crop=220:in_h:(in_w-220)/2:0" -c:v rawvideo -pix_fmt rgb565le 220_9fps.rgb`

### Animated GIF

#### 220x176@12fps

`ffmpeg -i input.mp4 -vf "fps=12,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" -loop -1 220_12fps.gif`

#### 220x176@15fps

`ffmpeg -i input.mp4 -vf "fps=15,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" -loop -1 220_15fps.gif`

### Motion JPEG

#### 220x176@24fps

`ffmpeg -i input.mp4 -vf "fps=24,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0" -q:v 9 220_24fps.mjpeg`

#### 220x176@30fps

`ffmpeg -i input.mp4 -vf "fps=30,scale=-1:176:flags=lanczos,crop=220:in_h:(in_w-220)/2:0" -q:v 9 220_30fps.mjpeg`

#### 320x240@12fps

`ffmpeg -i input.mp4 -vf "fps=12,scale=-1:240:flags=lanczos,crop=320:in_h:(in_w-320)/2:0" -q:v 9 320_12fps.mjpeg`
