package webm.internal;
import nme.display.Bitmap;
import nme.display.BitmapData;
import nme.display.PixelSnapping;
import nme.display.Sprite;

class WebmUtils {
	static public function replaceSpriteWithBitmapData(sprite:Sprite, bitmapData:BitmapData):Void {
		while (sprite.numChildren > 0) sprite.removeChildAt(0);
		sprite.addChild(new Bitmap(bitmapData, PixelSnapping.AUTO, true));
	}
}