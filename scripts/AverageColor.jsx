app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
var CurProj = app.newProject();
var ColorLut = [[1,0,0],[0,1,0],[0,0,1],[1,1,1]];


function hslToRgb(h, s, l) {
	var r, g, b;
	if (s == 0) {
		r = g = b = l;
	}
	else {
		var hue2rgb = function hue2rgb(p, q, t) {
			if (t < 0) t += 1;
			if (t > 1) t -= 1;
			if (t < 1 / 6) return p + (q - p) * 6 * t;
			if (t < 1 / 2) return q;
			if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
			return p;
		}

		var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
		var p = 2 * l - q;
		r = hue2rgb(p, q, h + 1 / 3);
		g = hue2rgb(p, q, h);
		b = hue2rgb(p, q, h - 1 / 3);
	}

	return [
		Math.floor(r * 0xFF),
		Math.floor(g * 0xFF),
		Math.floor(b * 0xFF)
	];
}

function GenComp(Config) {
	/// Config
	var Name = Config.Name;
	var Size = Config.Size;
	var RegisterWidth = Config.RegisterWidth;
	var CellSize = Config.CellSize;
	var IterDuration = Config.IterDuration;
	var Alignments = Config.Alignments;
	var PixelCount = Config.PixelCount;
	var PixelGrid = Config.PixelGrid;

	// Constants
	var TotalIterations = 0;
	var RemainingIterations = PixelCount;
	Alignments.slice(0).sort(
		function(a,b){return a.Width - b.Width;}
	).reverse().map(
		function (CurAlign, i) {
			if(CurAlign.Width <= 0){ TotalIterations++; return;}
			TotalIterations += Math.floor( RemainingIterations / CurAlign.Width );
			RemainingIterations %= CurAlign.Width;
			RemainingIterations = Math.floor(RemainingIterations);
		}
	);
	// Comp Items
	var CurComp = CurProj.items.addComp(
		Name,
		Math.floor(Size[0]),            // Width
		Math.floor(Size[1]),            // Height
		1,                              // Pixel aspect
		TotalIterations * IterDuration, // Duration
		50                              // framerate
	);
	CurComp.hideShyLayers = true;
	CurComp.bgColor = [1,1,1];
	// "Structs"
	function Pixel( Color ) {
		this.Channels = Color;
		var NewNull = CurComp.layers.addNull();
		NewNull.name = "Pixel";
		NewNull.opacity.setValue(100);
		// Center anchor point
		this.root = NewNull;

		// RGBA color bytes shapes
		this.shapes = [];
		for (var i = 0; i < 4; ++i)
		{
			var ColorByteShapeLayer = CurComp.layers.addShape();
			ColorByteShapeLayer.shy = true;
			this.shapes.push(ColorByteShapeLayer);
			ColorByteShapeLayer.name = Color[i];
			ColorByteShapeLayer.parent = NewNull;
			ColorByteShapeLayer.position.setValue([i * CellSize,0]);
			ColorByteShapeLayer.opacity.expression = "parent.opacity * opacity/100";
			// Group for each channel
			var ColorByteShapeGroup = ColorByteShapeLayer
				.property("Contents")
				.addProperty("ADBE Vector Group");
			
			var ChannelBytesShapeContents = ColorByteShapeGroup.property("Contents");
			var ChanneByte = ChannelBytesShapeContents
				.addProperty("ADBE Vector Shape - Rect");
			ChanneByte.property("ADBE Vector Rect Size")
				.setValue([CellSize * (9/10),CellSize * (9/10)]);
			ChanneByte.property("ADBE Vector Rect Roundness").setValue(CellSize / 10);
			// Border
			var Stroke = ChannelBytesShapeContents.addProperty("ADBE Vector Graphic - Stroke");
			Stroke.property("ADBE Vector Stroke Width").setValue(1.5);
			Stroke.property("ADBE Vector Stroke Color").setValue([0,0,0]);

			// Set color
			ChannelBytesShapeContents.addProperty("ADBE Vector Graphic - Fill").property("ADBE Vector Fill Color")
				.setValue(ColorLut[i].map(function(element){return (element*Color[i]) / 0xFF}));
			// Label
			var ByteLabel = CurComp.layers.addText(Color[i]);
			ByteLabel.name = Color[i];
			ByteLabel.parent = ColorByteShapeLayer;
			ByteLabel.position.setValue([0,0]);
			ByteLabel.opacity.expression = "parent.opacity * opacity/100";
			ByteLabel.shy = true;
			
			var LabelProp = ByteLabel.property("Source Text");
			var LabelText = LabelProp.value;
			LabelText.resetCharStyle();
			LabelText.text = Color[i];
			LabelText.fontSize = CellSize / 2;
			LabelText.fillColor = [0,0,0];
			LabelText.strokeColor = [0.5,0.5,0.5];
			LabelText.strokeWidth = 2;
			LabelText.applyStroke = true;
			LabelText.font = "Dina ttf 10px";
			LabelText.strokeOverFill = false;
			LabelText.justification = ParagraphJustification.CENTER_JUSTIFY;
			LabelProp.setValue(LabelText);
		}
	}

	function Register(Name, CellCount) {
		var NewNull = CurComp.layers.addNull();
		this.root = NewNull;
		NewNull.name = Name;
		// Center anchor point
		NewNull.anchorPoint.setValue([
			NewNull.width / 2, NewNull.height / 2
		]);
		var RegShapeLayer = CurComp.layers.addShape();
		RegShapeLayer.name = Name + "-Shape";
		this.shape = RegShapeLayer;
		RegShapeLayer.parent = NewNull;

		// Register Cells
		var ResCellShapeContents = RegShapeLayer
			.property("Contents")
			.addProperty("ADBE Vector Group")
			.property("Contents");

		for (var i = 0; i < CellCount; ++i) {
			var ResCellRect = ResCellShapeContents
				.addProperty("ADBE Vector Shape - Rect");
			ResCellRect.property("ADBE Vector Rect Position")
				.setValue([
					i * CellSize,
					0
				]);
			ResCellRect.property("ADBE Vector Rect Size")
				.setValue([
					CellSize * (9/10),
					CellSize * (9/10)
				]);
			ResCellRect.property("ADBE Vector Rect Roundness")
				.setValue(CellSize / 10);
		}

		ResCellShapeContents
			.addProperty("ADBE Vector Graphic - Fill")
			.property("ADBE Vector Fill Color")
			.setValue([0.44, 0.44, 0.44]);


		// Register Box
		var ResBoxShapeContents = RegShapeLayer
			.property("Contents")
			.addProperty("ADBE Vector Group")
			.property("Contents");

		var ResBoxRect = ResBoxShapeContents.addProperty("ADBE Vector Shape - Rect")
		ResBoxRect.property("ADBE Vector Rect Size")
			.setValue([
				CellSize * RegisterWidth + CellSize * (1/10),
				CellSize + CellSize * (1/10)
			]);
		ResBoxRect.property("ADBE Vector Rect Position")
			.setValue([(RegisterWidth / 2 - 1) * CellSize + CellSize / 2, 0]);
		ResBoxRect.property("ADBE Vector Rect Roundness")
			.setValue(CellSize *  (1/10));

		ResBoxShapeContents
			.addProperty("ADBE Vector Graphic - Fill")
			.property("ADBE Vector Fill Color")
			.setValue([0.33, 0.33, 0.33]);
		RegShapeLayer.locked=true;
	}
	function Accumulator(Name, CellCount, Value, Color){
		var NewNull = CurComp.layers.addNull();
		this.Value = Value;
		this.root = NewNull;
		NewNull.name = Name;
		// Center anchor point
		NewNull.anchorPoint.setValue([
			NewNull.width / 2, NewNull.height / 2
		]);
		var RegShapeLayer = CurComp.layers.addShape();
		RegShapeLayer.name = Name + "-Shape";
		this.shape = RegShapeLayer;
		RegShapeLayer.parent = NewNull;
		// Register Box
		var ResBoxShapeContents = RegShapeLayer
			.property("Contents")
			.addProperty("ADBE Vector Group")
			.property("Contents");

		var ResBoxRect = ResBoxShapeContents.addProperty("ADBE Vector Shape - Rect")
		// ResBoxRect.property("ADBE Vector Rect Position")
		// 	.setValue([(RegisterWidth / 2 - 1) * CellSize, 0]);
		ResBoxRect.property("ADBE Vector Rect Size")
			.setValue([
				CellSize * CellCount + CellSize * (1/10),
				CellSize + CellSize * (1/10)
			]);
		ResBoxRect.property("ADBE Vector Rect Roundness")
			.setValue(CellSize *  (1/10));

		ResBoxShapeContents
			.addProperty("ADBE Vector Graphic - Fill")
			.property("ADBE Vector Fill Color")
			.setValue([0.33, 0.33, 0.33]);
		// Border
		var Stroke = ResBoxShapeContents.addProperty("ADBE Vector Graphic - Stroke");
		Stroke.property("ADBE Vector Stroke Width").setValue(1.5);
		Stroke.property("ADBE Vector Stroke Color").setValue([0,0,0]);
		RegShapeLayer.locked=true;
		// Label
		var ValueLabel = CurComp.layers.addText(Value);
		ValueLabel.name = Name + "-Label";
		ValueLabel.parent = RegShapeLayer;
		ValueLabel.position.setValue(
			[
				0,
				CellSize / 4
			]
		);
		ValueLabel.shy = true;
		
		LabelProp = ValueLabel.property("Source Text");
		this.label=LabelProp;
		var LabelText = LabelProp.value;
		LabelText.resetCharStyle();
		LabelText.text = Value;
		LabelText.fontSize = CellSize;
		LabelText.fillColor = Color;
		LabelText.strokeColor = [0,0,0];
		LabelText.strokeWidth = 1;
		LabelText.applyStroke = true;
		LabelText.font = "Dina ttf 10px";
		LabelText.strokeOverFill = false;
		LabelText.justification = ParagraphJustification.CENTER_JUSTIFY;
		LabelProp.setValue(LabelText);
	}
	var MainRegister = new Register("MainRegister",RegisterWidth);
	MainRegister.root.position.setValue([(CurComp.width/2) - ((CellSize*RegisterWidth)/2),CurComp.height/2])
	
	var Sums = [];
	
	for (var i = 0; i < 4; ++i) {
		var SumNames = ["Red","Green","Blue","Alpha"];
		Sums.push(new Accumulator(SumNames[i] + "-Sum", 3, 0,ColorLut[i]))
		Sums[i].root.position.setValue([
			(CurComp.width/2) - (4 * 3 * CellSize)/2 + ( i * (4 * CellSize) ),
			CurComp.height/4
		]);
	}
	
	// Generate pixel grid
	var Pixels = [];
	for (var i = 0; i < PixelCount; ++i) {
		var CurPixel = new Pixel(hslToRgb(i / PixelCount,1.0,0.5).concat([0xFF]));
		CurPixel.root.name = "Pixel-" + i;
		CurPixel.root.position.setValue(
			[
				( CurComp.width / 2) - (( (CellSize * 4) * PixelGrid[0])/2) + (Math.floor(i%PixelGrid[0] * CellSize * 4)),
				( CurComp.height - CurComp.height / 3) + (Math.floor(i / PixelGrid[1]) * CellSize)
			]
		);
		Pixels.push(CurPixel);
	}

	var MethodState = {
		Register: MainRegister,
		Pixels: Pixels,
		CurPixel: 0,
		Methods: Alignments,
		CurMethod: Alignments.length - 1
	};
	
	var CurTime = 0;
	for (var i = 0; i < TotalIterations; ++i) {
		if( (MethodState.Pixels.length - MethodState.CurPixel)  < MethodState.Methods[MethodState.CurMethod].Width )
		{
			MethodState.CurMethod--;
		}
		var IterMethod = MethodState.Methods[MethodState.CurMethod];
		var TimeIn = i * IterDuration;
		var Marker = new MarkerValue(IterMethod.Name);
		Marker.duration = IterDuration;
		CurComp.markerProperty.setValueAtTime(TimeIn,Marker);
		//  Move pixels to register
		for (var j = 0; j < IterMethod.Width; ++j) {

			MethodState.Pixels[MethodState.CurPixel + j].root.position.setValueAtTime(
				TimeIn,
				MethodState.Pixels[MethodState.CurPixel + j].root.position.value
			);
			MethodState.Pixels[MethodState.CurPixel + j].root.position.setValueAtTime(
				TimeIn + IterDuration * 1/4,
				[
					MethodState.Register.root.position.value[0] + j * (4 * CellSize),
					MethodState.Register.root.position.value[1]
				]
			);

			// Smoothen keyframes
			for (var k = 1; k <= MethodState.Pixels[MethodState.CurPixel + j].root.position.numKeys; ++k) {
				MethodState.Pixels[MethodState.CurPixel + j].root.position.setInterpolationTypeAtKey(k,
					KeyframeInterpolationType.BEZIER, KeyframeInterpolationType.BEZIER
				);
				MethodState.Pixels[MethodState.CurPixel + j].root.position.setTemporalEaseAtKey(
					k,
					[new KeyframeEase(1 / 4, 100)]
				);
			}
		}
		// Pixel is now in register, call method function on them
		IterMethod.Proc(
			TimeIn + IterDuration * 2/4,
			IterDuration * 2 /4,
			IterMethod.Width != 0 ? MethodState.Pixels.slice(MethodState.CurPixel,MethodState.CurPixel + IterMethod.Width):MethodState.Pixels,
			Config,
			Sums
		);
		MethodState.CurPixel += IterMethod.Width;
	}
	// Alignments.map(function (CurAlign, i) {
	// 	SwapPhase(CurAlign, OpNames[i]);
	// });
	CurComp.openInViewer();
	return CurComp;
}

var SSEMethod = {
	Name: "SSE",
	Width: 4,
	Proc: function( TimeIn, Duration, Pixels, Config, Sums ){
		for (var k = 0; k < 4; ++k) { // Current pixel
			for (var i = 0; i < 4; ++i) { // Current channel
				// Process channel
				// Arming keys
				Pixels[k].shapes[i].opacity.setValueAtTime(TimeIn,100);
				Pixels[k].shapes[i].scale.setValueAtTime(TimeIn,[100,100]);
				Pixels[k].shapes[i].anchorPoint.setValueAtTime(TimeIn,[0,0]);
				Sums[i].label.setValueAtTime(TimeIn,Sums[i].Value);
				// Main keys
				Pixels[k].shapes[i].opacity.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),100);
				Pixels[k].shapes[i].scale.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),[125,125]); 
				Pixels[k].shapes[i].anchorPoint.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),[0,Config.CellSize/2]);
				Sums[i].Value += Pixels[0].Channels[i];
				Sums[i].label.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),Sums[i].Value);
				// Outro
				Pixels[k].shapes[i].opacity.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 4/4),0);
				Pixels[k].shapes[i].scale.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 4/4),[0,0]);
				Pixels[k].shapes[i].anchorPoint.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 4/4),[0,Config.CellSize*2]);
			}
		}
	}
}

var SerialMethod = {
	Name: "Serial",
	Width: 1,
	Proc: function( TimeIn, Duration, Pixels, Config, Sums ){
		for (var i = 0; i < 4; ++i) {
			// Process channel
			// Arming keys
			Pixels[0].shapes[i].opacity.setValueAtTime(TimeIn,100);
			Pixels[0].shapes[i].scale.setValueAtTime(TimeIn,[100,100]);
			Pixels[0].shapes[i].anchorPoint.setValueAtTime(TimeIn,[0,0]);
			Sums[i].label.setValueAtTime(TimeIn,Sums[i].Value);
			// Main keys
			Pixels[0].shapes[i].opacity.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),100);
			Pixels[0].shapes[i].scale.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),[125,125]); 
			Pixels[0].shapes[i].anchorPoint.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),[0,Config.CellSize/2]);
			Sums[i].Value += Pixels[0].Channels[i];
			Sums[i].label.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),Sums[i].Value);
			// Outro
			Pixels[0].shapes[i].opacity.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 4/4),0);
			Pixels[0].shapes[i].scale.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 4/4),[0,0]);
			Pixels[0].shapes[i].anchorPoint.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 4/4),[0,Config.CellSize*2]);
		}
	}
}
var Average = {
	Name: "Average",
	Width: 0,
	Proc: function( TimeIn, Duration, Pixels, Config, Sums ){
		for (var i = 0; i < 4; ++i) {
			// Process channel
			// Arming keys
			Sums[i].label.setValueAtTime(TimeIn,Sums[i].Value);
			// Main keys
			Sums[i].Value /= Pixels.length;
			Sums[i].Value = Math.floor(Sums[i].Value);
			Sums[i].label.setValueAtTime(TimeIn + ((i+1)/4) * (Duration * 2/4),Sums[i].Value);
			// Outro
		}
	}
}

// Serial
var Serial = GenComp({
	Name: "Serial",
	Size: [520, 360],
	RegisterWidth: 4,
	CellSize: 32,
	IterDuration: 0.5,
	Alignments: [
		Average,
		SerialMethod,
	],
	PixelCount: 13,
	PixelGrid: [4,3]
});

// Sad
var SAD = GenComp({
	Name: "SAD",
	Size: [520, 360],
	RegisterWidth: 16,
	CellSize: 24,
	IterDuration: 0.5,
	Alignments: [
		Average,
		SerialMethod,
		SSEMethod
	],
	PixelCount: 13,
	PixelGrid: [4,3]
});