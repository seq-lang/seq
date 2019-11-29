from ipykernel.kernelbase import Kernel
from subprocess import check_output
from io import BytesIO
import re

from .redirector import stdout_stderr_redirector
from .wrapper import SeqWrapper

__version__ = '0.0.0'

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
        'mimetype': 'application/seq',
        'file_extension': '.seq',
    }

    def __init__(self, **kwargs):
        Kernel.__init__(self, **kwargs)
        self.seqwrapper = SeqWrapper()

    def do_execute(self, code, silent, store_history=True, user_expressions=None, allow_stdin=False):
        if not code.strip():
            return {'status': 'ok', 'execution_count': self.execution_count,
                    'payload': [], 'user_expressions': {}}

        fout = BytesIO()
        ferr = BytesIO()
        
        with stdout_stderr_redirector(fout, ferr):
            self.seqwrapper.exec(code.rstrip())
        
        fout_string = fout.getvalue().decode('utf-8').strip()
        ferr_string = ferr.getvalue().decode('utf-8').strip()
        
        if ferr_string:
            if not silent:
                self.send_response(self.iopub_socket, 'stream', {'name': 'stderr', 'text': ferr_string})

            return {'status': 'error', 'execution_count': self.execution_count}
        
        else:
            if not silent:
                self.send_response(self.iopub_socket, 'stream', {'name': 'stdout', 'text': fout_string})

            return {'status': 'ok', 'execution_count': self.execution_count,
                    'payload': [], 'user_expressions': {}}

    def _get_object(self, code, cursor_pos):
        # TODO: Get the correct section of code to be inspected
        left = code[:cursor_pos].rsplit(' ', 1)[-1]
        right = code[cursor_pos:].split(' ', 1)[0]
        return left + right

    def do_inspect(self, code, cursor_pos, detail_level=0):
        data = self.seqwrapper.inspect(self._get_object(code, cursor_pos), detail_level)
        return {
            'status': 'ok',
            'found': True,
            'data': data,
            'metadata': {}
        }