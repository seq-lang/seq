from contextlib import contextmanager
import ctypes
import io
import os
import sys
import tempfile

libc = ctypes.CDLL(None)
c_stdout = ctypes.c_void_p.in_dll(libc, 'stdout')
c_stderr = ctypes.c_void_p.in_dll(libc, 'stderr')

@contextmanager
def stdout_stderr_redirector(out_stream, err_stream):
    # The original fd stdout points to. Usually 1 on POSIX systems.
    original_stdout_fd = sys.__stdout__.fileno()
    original_stderr_fd = sys.__stderr__.fileno()

    def _redirect_stdout(to_fd):
        """Redirect stdout to the given file descriptor."""
        # Flush the C-level buffer stdout
        libc.fflush(c_stdout)
        # Flush and close sys.stdout - also closes the file descriptor (fd)
        sys.__stdout__.close()
        # Make original_stdout_fd point to the same file as to_fd
        os.dup2(to_fd, original_stdout_fd)
        # Create a new sys.stdout that points to the redirected fd
        sys.__stdout__ = io.TextIOWrapper(os.fdopen(original_stdout_fd, 'wb'))

    def _redirect_stderr(to_fd):
        """Redirect stderr to the given file descriptor."""
        # Flush the C-level buffer stderr
        libc.fflush(c_stderr)
        # Flush and close sys.stderr - also closes the file descriptor (fd)
        sys.__stderr__.close()
        # Make original_stderr_fd point to the same file as to_fd
        os.dup2(to_fd, original_stderr_fd)
        # Create a new sys.stderr that points to the redirected fd
        sys.__stderr__ = io.TextIOWrapper(os.fdopen(original_stderr_fd, 'wb'))

    # Save a copy of the original stdout fd in saved_stdout_fd
    saved_stdout_fd = os.dup(original_stdout_fd)
    saved_stderr_fd = os.dup(original_stderr_fd)
    try:
        # Create a temporary file and redirect stdout to it
        tfile_out = tempfile.TemporaryFile(mode='w+b')
        tfile_err = tempfile.TemporaryFile(mode='w+b')
        _redirect_stdout(tfile_out.fileno())
        _redirect_stderr(tfile_err.fileno())
        # Yield to caller, then redirect stdout back to the saved fd
        yield
        _redirect_stdout(saved_stdout_fd)
        _redirect_stderr(saved_stderr_fd)
        # Copy contents of temporary file to the given stream
        tfile_out.flush()
        tfile_err.flush()
        tfile_out.seek(0, io.SEEK_SET)
        tfile_err.seek(0, io.SEEK_SET)
        out_stream.write(tfile_out.read())
        err_stream.write(tfile_err.read())
    finally:
        tfile_out.close()
        tfile_err.close()
        os.close(saved_stdout_fd)
        os.close(saved_stderr_fd)
