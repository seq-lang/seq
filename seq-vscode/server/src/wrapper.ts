import { IConnection } from 'vscode-languageserver';
import { DynamicLibrary, VariadicForeignFunction, ForeignFunction, types } from 'ffi';
import { isNull, refType } from 'ref';


function GlobalLibrary (libfile: string | undefined, funcs: any, lib?: any): any {
	/**
	 * GlobalLibrary
	 * ffi.Library rewritten to use more RTLD flags.
	 */
	if (process.platform === 'linux') {
		libfile += '.so';
	} else if (process.platform === 'darwin') {
		libfile += '.dylib';
	} else {
		throw new Error('The current platform is not supported');
	}
	if (!lib) {
		lib = {};
	}
	const dl: DynamicLibrary = new DynamicLibrary(
		libfile || undefined,
		DynamicLibrary.FLAGS.RTLD_NOW | DynamicLibrary.FLAGS.RTLD_GLOBAL
	);

	Object.keys(funcs || {}).forEach((func) => {
		const fptr: Buffer = dl.get(func)
			, info = funcs[func];
		
		if (isNull(fptr)) {
			throw new Error('Library: "' + libfile + '" returned NULL function pointer for "' + func + '"');
		}

		const resultType = info[0]
			, paramTypes = info[1]
			, fopts = info[2]
			, abi = fopts && fopts.abi
			, async = fopts && fopts.async
			, varargs = fopts && fopts.varags;
		
		if (varargs) {
			lib[func] = VariadicForeignFunction(fptr, resultType, paramTypes, abi);
		} else {
			const ff = ForeignFunction(fptr, resultType, paramTypes, abi);
			lib[func] = async ? ff.async :  ff;
		}
	});
	return lib;
}

export default class SeqWrapper {
	/**
	 * SeqWrapper
	 * Wrapper class for Seq library. Uses node-ffi and ref to interface with the '.so' file.
	 */
	connection: IConnection;
	lib: any;
	// handle: Buffer;
	constructor(connection: IConnection) {
		const voidPtr = refType(types.void);
		this.connection = connection;
		this.connection.console.log('hello from wrapper');
		// TODO: Better way of geting the path to libseqjit.so
		this.lib = GlobalLibrary('/path/to/libseqjit', {
			'seq_jit_init': [voidPtr, []],
			'seq_jit_complete': [types.CString, [voidPtr, types.CString]],
			'seq_jit_document': [types.CString, [voidPtr, types.CString]]
		});
		// TODO: The following line causes the server to crash.
		// this.handle = this.lib.seq_jit_init();
	}

	complete(prefix: string): string[] {
		return [];
	}

	document(idn: string): string {
		return '';
	}
}
