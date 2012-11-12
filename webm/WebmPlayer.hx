package webm;
import haxe.io.Bytes;
import haxe.io.BytesData;
import nme.display.BitmapData;
import nme.display.Sprite;
import nme.events.Event;
import nme.utils.ByteArray;
import webm.internal.WebmUtils;

class WebmPlayer {
	var webm:Webm;
	var io:WebmIo;
	var targetSprite:Sprite;
	var decoder:Dynamic;
	var startTime:Float = 0;
	var lastDecodedVideoFrame:Float = 0;

	public function new(io:WebmIo, targetSprite:Sprite) {
		this.io = io;
		this.targetSprite = targetSprite;
		this.webm = new Webm();
		this.decoder = hx_webm_decoder_create(io.io);
	}
	
	public function getElapsedTime():Float {
		return haxe.Timer.stamp() - this.startTime;
	}
	
	public function play() {
		this.startTime = haxe.Timer.stamp();

		stop();
		this.targetSprite.addEventListener("enterFrame", onSpriteEnterFrame);
	}

	public function stop() {
		this.targetSprite.removeEventListener("enterFrame", onSpriteEnterFrame);
	}
	
	private function onSpriteEnterFrame(e:Event) {
		while (hx_webm_decoder_has_more(decoder) && lastDecodedVideoFrame < getElapsedTime()) {
			hx_webm_decoder_step(decoder, decodeVideoFrame, decodeAudioFrame);
		}
	}

	private function decodeVideoFrame(time:Float, data:BytesData):Void {
		lastDecodedVideoFrame = time;
		//trace("DECODE VIDEO FRAME! " + getElapsedTime() + ":" + time);
		webm.decode(ByteArray.fromBytes(Bytes.ofData(data)));
		var bmp:BitmapData = webm.getFrame();
		if (bmp != null) {
			WebmUtils.replaceSpriteWithBitmapData(this.targetSprite, bmp);
		}
	}
	
	private function decodeAudioFrame(time:Float, data:BytesData):Void {
		//trace("DECODE AUDIO FRAME! " + getElapsedTime() + ":" + time);
	}
	
	static var hx_webm_decoder_create = cpp.Lib.load("nme-webm", "hx_webm_decoder_create", 1);
	static var hx_webm_decoder_has_more = cpp.Lib.load("nme-webm", "hx_webm_decoder_has_more", 1);
	static var hx_webm_decoder_step = cpp.Lib.load("nme-webm", "hx_webm_decoder_step", 3);
}