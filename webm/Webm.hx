package webm;
import haxe.io.Bytes;
import haxe.io.BytesData;
import nme.display.Bitmap;
import nme.display.PixelSnapping;
import nme.display.Sprite;
import nme.events.Event;
import nme.utils.Endian;
import nme.utils.Timer;
import sys.io.File;
import sys.io.FileInput;
import webm.internal.WebmUtils;
#if (cpp || neko)
import neash.utils.ByteArray;
#else
import nme.utils.ByteArray;
#end
import nme.display.BitmapData;
import nme.errors.Error;
import nme.geom.Rectangle;

class Webm {
	static public function getVpxDecoderVersion():String {
		return hx_vpx_codec_iface_name();
	}
	
	private var context:Dynamic;
	
	public function new() {
		trace("Initializing webm...");
		this.context = hx_vpx_codec_dec_init();
	}
	
	public function decode(data:ByteArray) {
		var info:Array<Int> = hx_vpx_codec_decode(this.context, data.getData());
		var width:Int = info[0];
		var height:Int = info[1];
		var isKeyFrame:Bool = (info[2] != 0);
		//trace(Std.format("decode($width, $height, $isKeyFrame)"));
	}

	public function getFrame():BitmapData {
		var info:Array<Dynamic> = hx_vpx_codec_get_frame(this.context);
		
		if (info == null) return null;
		
		var width:Int = info[0];
		var height:Int = info[1];
		var ba:ByteArray = ByteArray.fromBytes(Bytes.ofData(info[2]));
		var bmp:BitmapData = new BitmapData(width, height);
		bmp.setPixels(bmp.rect, ba);
		//trace(width);
		//trace(height);
		//trace(ba.length);
		return bmp;
	}

	static inline var IVF_FILE_HDR_SZ:Int = (32);
	static inline var IVF_FRAME_HDR_SZ:Int = (12);
	
	/**
	 * Decodes a webm video file.
	 * 
	 * @param	filePath
	 * @param	sprite
	 */
	public function decodeWebm(filePath:String, sprite:Sprite):Void {
		var fileInput:FileInput = File.read(filePath, true);
	}

	/**
	 * Decodes a raw V8 video file.
	 * 
	 * @param	filePath
	 * @param	sprite
	 */
	public function decodeIvf(filePath:String, sprite:Sprite):Void {
		var fileInput:FileInput = File.read(filePath, true);
		var header:ByteArray = ByteArray.fromBytes(fileInput.read(IVF_FILE_HDR_SZ));
		header.endian = Endian.LITTLE_ENDIAN;
		if (header.readUTFBytes(4) != "DKIF") throw(new Error("Not a IVF raw V8 file"));
		var fileVersion:Int = header.readUnsignedShort();
		header.readUnsignedShort();
		
		var fourcc:Int = header.readUnsignedInt();
		var width:Int = header.readUnsignedShort();
		var height:Int = header.readUnsignedShort();
		var fpsNum:Int = header.readInt();
		var fpsDen:Int = header.readInt();
		
		//trace(Std.format("${width}x${height}"));
		//trace(Std.format("$fpsNum/$fpsDen"));
		
		if (fpsNum < 1000) {
			// Correct for the factor of 2 applied to the timebase in the
			// encoder.
			if ((fpsNum & 1) != 0) {
				fpsNum <<= 1;
			} else {
				fpsNum >>= 1;
			}
		} else {
			// Don't know FPS for sure, and don't have readahead code
			// (yet?), so just default to 30fps.
			fpsNum = 30;
			fpsDen = 1;
		}

		var fps:Float = fpsNum / fpsDen;
		
		var decodeIvfTick:Dynamic = null;
		decodeIvfTick = function() {
			// Schedule next decoding.
			haxe.Timer.delay(decodeIvfTick, Std.int(1000 / fps));

			while (!fileInput.eof()) {
				var frameHeader:ByteArray = ByteArray.fromBytes(fileInput.read(IVF_FRAME_HDR_SZ));
				frameHeader.endian = Endian.LITTLE_ENDIAN;
				var frameSize:Int = frameHeader.readInt();
				
				//trace("Frame: " + fi.tell() + " : " + frameSize);
				
				var frameBytes:ByteArray = ByteArray.fromBytes(fileInput.read(frameSize));
				if (frameBytes.length != frameSize) throw(new Error("Invalid data"));

				decode(frameBytes);
				var frameBitmapData:BitmapData;
				var emittedImage:Bool = false;
				while ((frameBitmapData = getFrame()) != null) {
					WebmUtils.replaceSpriteWithBitmapData(sprite, frameBitmapData);
					emittedImage = true;
				}
				
				if (emittedImage) break;
			}
		};

		decodeIvfTick();
	}
	
	static public function createIo(read:Int -> BytesData, seek:Float -> Int -> Int, tell:Void -> Float):Dynamic {
		return hx_create_io(read, seek, tell);
	}
	
	static public function testDecodeIvfFile(inputFileName:String, outputFileName:String) {
		hx_vpx_test_decode_main(inputFileName, outputFileName);
	}
	
	static var hx_vpx_codec_iface_name:Void -> String = cpp.Lib.load("nme-webm", "hx_vpx_codec_iface_name", 0);
	static var hx_vpx_codec_dec_init:Void -> Dynamic = cpp.Lib.load("nme-webm", "hx_vpx_codec_dec_init", 0);
	static var hx_vpx_codec_decode = cpp.Lib.load("nme-webm", "hx_vpx_codec_decode", 2);
	static var hx_vpx_codec_get_frame = cpp.Lib.load("nme-webm", "hx_vpx_codec_get_frame", 1);
	static var hx_vpx_test_decode_main = cpp.Lib.load("nme-webm", "hx_vpx_test_decode_main", 2);
	static var hx_create_io = cpp.Lib.load("nme-webm", "hx_create_io", 3);
}