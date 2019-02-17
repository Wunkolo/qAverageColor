import os
from subprocess import call
import numpy as np
import itertools
from PIL import Image
from PIL import ImageDraw

def chunks(List, Widths):
	i = 0
	for CurWidth in Widths:
		while i + CurWidth < len(List):
			yield List[i:i + CurWidth]
			i += CurWidth

# Params:
# Name: Name of generated frames. Default "Serial"
# Path: Path for output images Default: "./frames/(Name)/"
# Size: Of the image, default (100,100)
# Scale: Scaling for the resulting image. Default: 2
# Granularity: List of widths in which elements may be processed in parallel
#              Default: [1]
def Render( Params ):
	# Params
	Name = Params.get("Name", "Serial")
	Size = Params.get("Size", (100, 100))
	Path = "./frames/" + Name + "/"
	Scale = Params.get("Scale", 2)
	Granularity = Params.get("Granularity", [1])
	# Sort by largest to smallest
	Granularity.sort()
	Granularity.reverse()
	# Create target path recursively
	os.makedirs(Path, exist_ok=True)
	# Create image
	Img = Image.new('RGB', Size)
	Draw = ImageDraw.Draw(Img)
	# Generate each row of points up-front
	Points = [ (x,y) for y in range(Size[1]) for x in range(Size[0])]
	i = 0
	for CurPoints in chunks(Points,Granularity):
		# Hilight the pixels being currently processed
		Draw.point(
			CurPoints,
			fill=(0xFF, 0x00, 0x00)
		)
		Img.resize(
			(Img.width * Scale, Img.height * Scale),
			Image.NEAREST
		).save(Path + Name + '_' + str(i).zfill(6) + ".png")
		i += 1
		# Save the "processed" frame
		Draw.point(
			CurPoints,
			fill=(0xFF, 0xFF, 0xFF)
		)
		Img.resize(
			(Img.width * Scale, Img.height * Scale),
			Image.NEAREST
		).save(Path + Name + '_' + str(i).zfill(6) + ".png")
		i += 1
		
	call(
		[
			'ffmpeg',
			'-framerate','50',
			'-i', Path + Name + '_%6d.png',
			Name + '.gif',
			'-t','4'
		]
	)


# Serial
Render(
	{
		"Name": "Pat-Serial",
		"Granularity": [1],
		"Scale": 2,
		"Size": (100, 100)
	}
)

# SSE/NEON
Render(
	{
		"Name": "Pat-SSE",
		"Granularity": [4,1],
		"Scale": 2,
		"Size": (100, 100)
	}
)

# AVX2
Render(
	{
		"Name": "Pat-AVX2",
		"Granularity": [8,4,1],
		"Scale": 2,
		"Size": (100, 100)
	}
)

# AVX512
Render(
	{
		"Name": "Pat-AVX512",
		"Granularity": [16,8,4,1],
		"Scale": 2,
		"Size": (100, 100)
	}
)