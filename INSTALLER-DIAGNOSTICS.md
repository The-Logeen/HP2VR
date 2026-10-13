# Installer diagnostics — October 12

Use Install-HP2VR.cmd, then Check requirements or Install / update. Each operation
creates a separate HP2VR-Setup-date-time-id directory in your Windows temporary
folder. Click Open diagnostic logs after the operation finishes or fails.

Files to retain together:
- request.json: chosen inputs and operation mode.
- transcript.txt: timestamps, runtime/OS version, phases, checks and copy details.
- worker.out / worker.err: raw process output, including errors before the worker
  can initialize. PowerShell is launched in text mode; first-use progress is suppressed.
- result.json: explicit success/failure, phase, exception type/message, error ID,
  category, target, source location, stack, nested exception detail and next step.
- process-exit.txt: actual child exit status, separate from its completion report.
- worker.json: component-level requirement report for Check requirements.
- launch-error.txt: process-creation failure, when applicable.

A missing completion report means the worker did not complete normally. It is
not treated as success or attributed to antivirus without evidence. A negative
requirements check is distinct from a crashed worker. An incomplete install keeps
its INSTALL-INCOMPLETE marker; preserve it and choose a new destination to retry.

Startup/UI errors are saved separately as HP2VR-Setup-GUI-error-*.txt in the same
Windows temporary folder. Policy blocks that prevent ALL scripts from starting
cannot be logged by those scripts: retain the terminal error shown by the .cmd.
Power loss, disk-full and permission problems may prevent logs from being written.

Review logs before sharing: they include Windows usernames in local paths and
file names. No whole environment dump, credentials, save contents or automatic
uploads are collected. Temporary-folder cleanup can remove logs, so copy the
diagnostic folder somewhere safe when reporting a problem.
