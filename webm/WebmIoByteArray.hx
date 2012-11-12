package webm;
import haxe.io.BytesData;
import sys.io.File;
import sys.io.FileInput;

class WebmIoByteArray extends WebmIo {
	var data:ByteArray;
	var fileInput:FileInput;

	public function new(data:ByteArray) {
		this.data = data;
		this.io = Webm.createIo(this.read, this.seek, this.tell);
	}
	
	private function read(count:Int):BytesData {
		var out:ByteArray = new ByteArray();
		this.data.readBytes(out, 0, count);
		return out.getData();
	}
	
	private function seek(offset:Float, whence:Int):Int {
		switch (whence) {
			case 0: this.data.position = Std.int(offset);
			case 1: this.data.position = Std.int(this.data.position + offset);
			case 2: this.data.position = Std.int(this.data.length + offset);
		}
		return 0;
	}
	
	private function tell():Float {
		return this.data.position;
	}
}