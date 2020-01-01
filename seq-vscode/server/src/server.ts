import {
	createConnection,
	TextDocuments,
	InitializeParams,
	DidChangeConfigurationNotification,
	CompletionItem,
	TextDocumentPositionParams,
	Hover
} from 'vscode-languageserver';

import SeqWrapper from './wrapper';

let connection = createConnection();

let wrapper: SeqWrapper = new SeqWrapper();

let documents: TextDocuments = new TextDocuments();

let hasConfigurationCapability: boolean = false;
let hasWorkspaceFolderCapability: boolean = false;

connection.onInitialize((params: InitializeParams) => {
	let capabilities = params.capabilities;

	hasConfigurationCapability = !!(
		capabilities.workspace && !!capabilities.workspace.configuration
	);
	hasWorkspaceFolderCapability = !!(
		capabilities.workspace && !!capabilities.workspace.workspaceFolders
	);

	return {
		capabilities: {
			textDocumentSync: documents.syncKind,
			completionProvider: {
				resolveProvider: true
			},
			hoverProvider: true
		}
	};
});

connection.onInitialized(() => {
	if (hasConfigurationCapability) {
		connection.client.register(DidChangeConfigurationNotification.type, undefined);
	}
	if (hasWorkspaceFolderCapability) {
		connection.workspace.onDidChangeWorkspaceFolders(_event => {
			connection.console.log('Workspace folder change event received.');
		});
	}
});

interface ExampleSettings {
	maxNumberOfProblems: number;
}

const defaultSettings: ExampleSettings = { maxNumberOfProblems: 1000 };
let globalSettings: ExampleSettings = defaultSettings;

let documentSettings: Map<string, Thenable<ExampleSettings>> = new Map();

connection.onDidChangeConfiguration(change => {
	if (hasConfigurationCapability) {
		documentSettings.clear();
	} else {
		globalSettings = <ExampleSettings>(
			(change.settings.languageServerSeq || defaultSettings)
		);
	}
});

documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});

connection.onDidChangeWatchedFiles(_change => {
	connection.console.log('We received an file change event');
});

connection.onCompletion(
	(_textDocumentPosition: TextDocumentPositionParams): CompletionItem[] => {
		connection.console.log(`onCompletion ${_textDocumentPosition.textDocument.uri} ${_textDocumentPosition.position.line} ${_textDocumentPosition.position.character}`);
		const prefix = documents.get(_textDocumentPosition.textDocument.uri)?.getText({
			start: { line: _textDocumentPosition.position.line, character: 0 },
			end: { line: _textDocumentPosition.position.line, character: _textDocumentPosition.position.character }
		});
		if (!prefix) {
			return [];
		}
		connection.console.log(`prefix ${prefix}`);
		// TODO: Use the seq wrapper to get the list of completion items.
		return [];
	}
);

connection.onCompletionResolve(
	(item: CompletionItem): CompletionItem => {
		connection.console.log(`onCompletionResolve ${item}`);
		return item;
	}
);

connection.onHover(
	(_textDocumentPosition: TextDocumentPositionParams): Hover => {
		const prefix = documents.get(_textDocumentPosition.textDocument.uri)?.getText({
			start: { line: _textDocumentPosition.position.line, character: 0 },
			end: { line: _textDocumentPosition.position.line, character: _textDocumentPosition.position.character }
		});
		if (!prefix) {
			return {
				contents: {
					kind: 'plaintext',
					value: ''
				}
			};
		}
		connection.console.log(`prefix ${prefix}`);
		// TODO: Use the seq wrapper to get the hover text.
		return {
			contents: {
				kind: 'plaintext',
				value: ''
			}
		};
	}
)

connection.onDidOpenTextDocument((params) => {
	connection.console.log(`${params.textDocument.uri} opened.`);
});
connection.onDidChangeTextDocument((params) => {
	connection.console.log(`${params.textDocument.uri} changed: ${JSON.stringify(params.contentChanges)}`);
});
connection.onDidCloseTextDocument((params) => {
	connection.console.log(`${params.textDocument.uri} closed.`);
});


documents.listen(connection);

connection.listen();
