import { ForeignFunction, DynamicLibrary, RTLD_GLOBAL } from 'ffi';
import { refType, types } from 'ref';

export default class SeqWrapper {
	// private handle: ForeignFunction;
	constructor() {
		// const dl = new DynamicLibrary('/path/to/libseqjit.so', RTLD_GLOBAL);
		// const init_fn = new ForeignFunction(dl.get('seq_jit_init'), refType(types.void), []);
		// this.handle = init_fn();
	}
	complete(prefix: string): string[] {
		return [];
	}
	document(idn: string): string {
		return '';
	}
}
