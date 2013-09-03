package webm;
import haxe.io.BytesData;
import sys.io.File;
import sys.io.FileInput;

class WebmIoFile extends WebmIo {
	var filePath:String;
	var fileInput:FileInput;

	public function new(filePath:String) {
		this.filePath = filePath;
		this.fileInput = File.read(filePath, true);
		this.io = Webm.createIo(this.read, this.seek, this.tell);
	}
	
	private function read(count:Int):BytesData {
		return this.fileInput.read(count).getData();
	}
	
	private function seek(offset:Float, whence:Int):Int {
		this.fileInput.seek(Std.int(offset), switch (whence) {
			case 0: SeekBegin;
			case 1: SeekCur;
			case 2: SeekEnd;
			default: SeekCur;
		});
		return 0;
	}
	
	private function tell():Float {
		return this.fileInput.tell();
	}
}