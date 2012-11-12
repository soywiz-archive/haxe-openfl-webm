package webm;
import haxe.io.Bytes;
import haxe.io.BytesData;
import nme.display.Bitmap;
import nme.display.BitmapData;
import nme.display.PixelSnapping;
import nme.display.Sprite;
import nme.events.Event;
import nme.events.EventDispatcher;
import nme.utils.ByteArray;
import webm.internal.WebmUtils;

class WebmPlayer extends EventDispatcher {
	var webm:Webm;
	var io:WebmIo;
	var targetSprite:Sprite;
	var bitmap:Bitmap;
	var bitmapData:BitmapData;
	var decoder:Dynamic;
	var startTime:Float = 0;
	var lastDecodedVideoFrame:Float = 0;
	var playing:Bool = false;
	
	public var width:Int;
	public var height:Int;
	public var frameRate:Float;

	public function new(io:WebmIo, targetSprite:Sprite) {
		super();
		this.io = io;
		this.targetSprite = targetSprite;
		this.webm = new Webm();
		this.decoder = hx_webm_decoder_create(io.io);
		var info = hx_webm_decoder_get_info(this.decoder);
		this.width = info[0];
		this.height = info[1];
		this.frameRate = info[2];
		this.bitmapData = new BitmapData(this.width, this.height);
		this.bitmap = new Bitmap(this.bitmapData, PixelSnapping.AUTO, true);
		targetSprite.addChild(this.bitmap);
	}
	
	public function getElapsedTime():Float {
		return haxe.Timer.stamp() - this.startTime;
	}
	
	public function play() {
		if (!playing) {
			this.startTime = haxe.Timer.stamp();

			this.targetSprite.addEventListener("enterFrame", onSpriteEnterFrame);
			playing = true;
			this.dispatchEvent(new Event('play'));
		}
	}

	public function stop() {
		if (playing) {
			this.targetSprite.removeEventListener("enterFrame", onSpriteEnterFrame);
			playing = false;
			this.dispatchEvent(new Event('stop'));
		}
	}
	
	private function onSpriteEnterFrame(e:Event) {
		while (hx_webm_decoder_has_more(decoder) && lastDecodedVideoFrame < getElapsedTime()) {
			hx_webm_decoder_step(decoder, decodeVideoFrame, decodeAudioFrame);
		}
		
		if (!hx_webm_decoder_has_more(decoder)) {
			this.dispatchEvent(new Event('end'));
			stop();
		}
	}

	private function decodeVideoFrame(time:Float, data:BytesData):Void {
		lastDecodedVideoFrame = time;
		//trace("DECODE VIDEO FRAME! " + getElapsedTime() + ":" + time);
		webm.decode(ByteArray.fromBytes(Bytes.ofData(data)));
		webm.getAndRenderFrame(this.bitmapData);
	}
	
	private function decodeAudioFrame(time:Float, data:BytesData):Void {
		//trace("DECODE AUDIO FRAME! " + getElapsedTime() + ":" + time);
	}
	
	static var hx_webm_decoder_create = cpp.Lib.load("nme-webm", "hx_webm_decoder_create", 1);
	static var hx_webm_decoder_get_info = cpp.Lib.load("nme-webm", "hx_webm_decoder_get_info", 1);
	static var hx_webm_decoder_has_more = cpp.Lib.load("nme-webm", "hx_webm_decoder_has_more", 1);
	static var hx_webm_decoder_step = cpp.Lib.load("nme-webm", "hx_webm_decoder_step", 3);
}