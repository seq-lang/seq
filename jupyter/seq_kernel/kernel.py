from ipykernel.kernelbase import Kernel
from subprocess import check_output
from io import BytesIO
import re

from .redirector import stdout_stderr_redirector
from .wrapper import SeqWrapper

__version__ = '0.0.1'

version_pat = re.compile(r'version (\d+(\.\d+)+)')

class SeqKernel(Kernel):
    implementation = 'seq_kernel'
    implementation_version = __version__

    @property
    def language_version(self):
        m = version_pat.search(self.banner)
        return m.group(1)

    _banner = None

    @property
    def banner(self):
        if self._banner is None:
            self._banner = check_output(['seqc', '--version']).decode('utf-8')
        return self._banner

    language_info = {
        'name': 'seq',
        'mimetype': 'text/seq',
        'file_extension': '.seq',
    }

    def __init__(self, **kwargs):
        Kernel.__init__(self, **kwargs)
        self.seqwrapper = SeqWrapper()
        self.cells = []

    def do_execute(self, code, silent, store_history=True, user_expressions=None, allow_stdin=False):
        """
            Execute user code.
            Parameters:
                code (str) -- The code to be executed.
                silent (bool) -- Whether to display output.
                store_history (bool) -- Whether to record this code in history and increase the execution count.
                    If silent is True, this is implicitly False.
                user_expressions (dict) -- Mapping of names to expressions to evaluate after the code has run.
                    You can ignore this if you need to.
                allow_stdin (bool) -- Whether the frontend can provide input on request (e.g. for Python's raw_input()).
        """
        if not code.strip():
            return {
                'status': 'ok',
                'execution_count': self.execution_count,
                'payload': [],
                'user_expressions': {}
            }

        fout = BytesIO()
        ferr = BytesIO()

        with stdout_stderr_redirector(fout, ferr):
            code = code.rstrip()
            self.seqwrapper.exec(code)
            self.cells.append(code)

        fout_string = fout.getvalue().decode('utf-8').strip()
        ferr_string = ferr.getvalue().decode('utf-8').strip()

        if ferr_string:
            if not silent:
                self.send_response(
                    self.iopub_socket,
                    'stream',
                    {'name': 'stderr', 'text': ferr_string}
                )
            return {
                'status': 'error',
                'ename': 'ExecutonError',
                'evalue': fout_string,
                'traceback': [],
                'execution_count': self.execution_count
            }

        if not silent:
            if fout_string.startswith('\0x1'):
                mime_type, data = fout_string.strip('\0x1').split('\b', 1)
                self.send_response(
                    self.iopub_socket,
                    'display_data',
                    {'data': {mime_type: data}, 'metadata': {}}
                )
            else:
                self.send_response(
                    self.iopub_socket,
                    'stream',
                    {'name': 'stdout', 'text': fout_string}
                )

        return {
            'status': 'ok',
            'execution_count': self.execution_count,
            'payload': [],
            'user_expressions': {}
        }
    
    def do_complete(self, code, cursor_pos):
        """
            Code completion.
            Parameters:
                code (str) -- The code already present.
                cursor_pos (int) -- The position in the code where completion is requested.
        """

        matches = []
        ferr = BytesIO()
        with stdout_stderr_redirector(BytesIO(), ferr):
            matches = self.seqwrapper.complete(code)

        ferr_string = ferr.getvalue().decode('utf-8').strip()

        if ferr_string:
            return {
                'status': 'error',
                'ename': 'CompletionError',
                'evalue': ferr_string,
                'traceback': []
            }
        
        return {
            'status': 'ok',
            'matches': [ match for match in matches],
            'cursor_start': 0,
            'cursor_end': cursor_pos,
            'metadata': {}
        }

    def do_inspect(self, code, cursor_pos, detail_level=0):
        """
            Object introspection.
            Parameters:
                code (str) -- The code.
                cursor_pos(int) -- The position in the code where introspection is requested.
                detail_level (int) -- 0 or 1 for more or less detail.
                    In IPython, 1 gets the source code.
        """

        inspect_text = ''
        doc_text = ''
        ferr = BytesIO()
        cell = 0
        line = 0
        col = 0

        if code in self.cells:
            cell = self.cells.index(code) + 1
            line = code[:cursor_pos].count('\n') + 1
            col = cursor_pos - code[:cursor_pos].rfind('\n') - 1
            if col < 0:
                col = 0
            

        with stdout_stderr_redirector(BytesIO(), ferr):
            if cell > 0:
                inspect_text = self.seqwrapper.inspect(cell, line, col)
            if cell <= 0 or inspect_text.startswith('Not found:'):
                doc_text = self.seqwrapper.document(code)
                inspect_text = ''

        ferr_string = ferr.getvalue().decode('utf-8').strip()

        if ferr_string:
            return {
                'status': 'error',
                'ename': 'InspectionError',
                'evalue': ferr_string,
                'traceback': []
            }
        
        if inspect_text:
            # TODO: This piece of code will break on any format changes from the jit side.
            obj_name, obj_type, obj_loc, obj_doc = inspect_text[7:].split('\b')
            obj_cell, obj_line, obj_col = obj_loc.split(':')
            obj_cell = obj_cell.split('_')[1][:-1]
            inspect_obj = [
                f'Name: {obj_name}',
                f'Type: {obj_type}',
                f'Defined at: In [{obj_cell}] line: {obj_line} col: {obj_col}',
                f'Documentation: {obj_doc}'
            ]

            return {
                'status': 'ok',
                'found': True,
                'data': {'text/plain': "\n".join(inspect_obj)},
                'metadata': {}
            }
        
        if doc_text:
            return {
                'status': 'ok',
                'found': True,
                'data': {'text/plain': f'Documentation: {doc_text}'},
                'metadata': {}
            }

        return {
            'status': 'ok',
            'found': False,
            'data': {},
            'metadata': {}
        }
