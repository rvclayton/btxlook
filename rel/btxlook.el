; btxlook.el - an emacs major mode for finding and inserting bibtex
; bibliographic citations in documents.  
;
; To Install:
;
;   First, make sure the directory containing the btxlook program is part of
; your PATH environment variable.
;
;   To load btxlook mode, put "(require 'btxlook)" in your .emacs file (usually
; as part of latex-mode-hook).  Make sure the directory containing btxlook.el
; is included in the load-path emacs variable.  Those in the know can also use
; autoload to load btxlook mode.
;
;   The following is a simple example of code from a .emacs file that loads
; and sets up btxlook mode (by calling btxlook-mode) whenever latex mode is
; entered:
;
;   (setq latex-mode-hook
;      '(lambda ()
;         (require 'btxlook)
;	  (btxlook-mode)
;         (local-set-key "\e\C-b" 'btxlook-search)
;         (setq btxlook-insert-single-match t)
;       )
;   )
;
; It also binds the btxlook-search command (see "To Use" below) to the
; keystrokes "<meta-ctrl-b>" and sets the default to automatically insert
; single matches (see "To Customize" below).
;
; To Use:
;
;   Load btxlook mode and type "<meta-x>btxlook-search". You will be prompted
; for a list of words, which are sent to btxlook.  If btxlook finds any
; matches, they are displayed in the match buffer; otherwise you get
; mini-buffer message indicating why there were no matches.  Once in the match
; buffer, type "<meta-x>help<ret>m" to see the buffer-specific commands.  The
; usual text mode commands, such as searching and buffer writing commands, are
; also available, but the match buffer is usually read-only, so commands that
; modify the buffer either will be missing or won't work properly.
;
;   btxlook search words are kept in a history list available at the "Search
; for:" prompt.  Type "<meta-p>" to display the previous word list; "<meta-n>"
; to display the next word list.  "<meta-r>" searches backwards through the
; history list to find a word list matching a regular expression; "<meta-s>"
; searches forward through the history list.
;
;   Each reference in the buffer is preceeded by the name of the bibliography
; file from which it came.  A marked reference has its file name preceeded by
; "* ".  Multiple references are inserted as a single \cite{} command.
;
; To Customize:
;
;   Variables not given in this list can (and probably should) be ignored.  For
; documentation, see the variable definitions below, or load btxlook mode and
; type "<meta-x>help<ret>v" followed by the variable name.
; 
;     btxlook-citation-cs
;     btxlook-insert-single-match
;
; To Fix:
;
; Cannot open load file: btxlook
;
;   Either the load-path emacs variable doesn't have the directory containing
;   btxlook.el or btxlook.el hasn't been installed.
;
; Searching for program: no such file or directory, btxlook
;
;   Either 1) the btxlook program hasn't been installed or 2) your PATH
;   environment variable isn't set to find btxlook.  Fixing things so you can
;   run btxlook from the shell should fix this problem too.
;
; Buffer btxlook process buffer has no process
;
;   btxlook-mode hasn't been called yet.  Type "<meta-x>btxlook-mode<ret>" to
;   fix.
;
; 16 dec 94  r. clayton, clayton@cc.gatech.edu.  Created.
;
;  4 jul 96  r. clayton, clayton@cc.gatech.edu.  Have a non-default numeric
;            argument to btxlook-search override btxlook-insert-single-match. 
;
; $Revision: 1.1.1.1 $, $Date: 2006-01-08 19:56:46 $


(defvar btxlook-citation-cs "cite"
"*The latex control sequence to use for a citation.  The control sequence
should not contain a leading '\\'."
)

(defvar btxlook-insert-single-match nil
"*If non-nil, then whenever a search has exactly one match, automatically
insert the match as a citation at point rather than switching to the match
buffer.  If nil, a successful search always switches to the match buffer.  Once
in the match buffer, single-match results are not automatically inserted.  A
non-default numeric argument to btxlook-search overrides
btxlook-insert-single-match and always switches to the match buffer."
)

(defvar btxlook-mode-map
  (let ((m (make-keymap)))
    (suppress-keymap m)
    (define-key m " " 'btxlook-scroll-up)
    (define-key m "e" 'btxlook-exit)
    (define-key m "h" 'btxlook-help)
    (define-key m "i" 'btxlook-insert-reference)
    (define-key m "m" 'btxlook-mark-reference)
    (define-key m "n" 'btxlook-next-reference)
    (define-key m "p" 'btxlook-previous-reference)
    (define-key m "q" 'btxlook-quit)
    (define-key m "s" 'btxlook-research)
    (define-key m "u" 'btxlook-update)
    m
  )
  "btxlook mode's key map"
)

(defvar btxlook-process nil "The btxlook process")

(defvar btxlook-process-buffer (get-buffer-create "btxlook process buffer")
  "Where the btxlook output goes."
)

(defvar btxlook-search-command-history nil "The history of search commands.")


(defun btxlook-count-references ()

"Return a cons cell giving the number of references in the buffer.  The car is
the number of references from the beginning of the buffer up to and including
the reference containing point; the cdr is the number of references following
the reference containing point to the end of the buffer."

  (let ((front 0)
	(back 0)
	(here (point))
	(end (point-max))
       )

    (save-excursion
      (goto-char (point-min))
      (while (search-forward-regexp "^@" here t 1)
	(setq front (1+ front)))

      (goto-char here)
      (while (search-forward-regexp "^@" end t 1)
	(setq back (1+ back)))
    )

    ; If point is in a reference but before the '@' marking the reference's
    ; beginning (which is usually the case), the code above considers the
    ; reference to be one of the ones after point.  If that happens, adjust the
    ; counts accordingly.

    (if (looking-at "\\(.*\n\\)?@") (cons (1+ front) (1- back))
      (cons front back))
  )
)


(defun btxlook-do-search (words)

"Submit WORDS to the btxlook process.  Return when btxlook has finished its
reply."

  (setq buffer-read-only nil)
  (erase-buffer)

  (setq btxlook-command words)
  (process-send-string btxlook-process (concat words "\n"))

  (btxlook-waitfor-prompt)

  (goto-char (point-min))
  (while (looking-at "\n") (kill-line 1))
  (if (looking-at "/") (beginning-of-line 2))

  (setq buffer-read-only t)
  (btxlook-make-mode-line)
)


(defun btxlook-exit ()

"Insert all marked references at point in the previous buffer and return to the
previous buffer."

  (interactive)

  (let (keys (list))
    (goto-char (point-min))
    (while (re-search-forward "^\\* /" (point-max) t)
      (setq keys (cons (btxlook-get-key) keys)))

    (set-buffer btxlook-previous-buffer)
    (btxlook-insert-citations keys)
    (btxlook-quit)
  )
)


(defun btxlook-get-key ()

"Return the key of the reference containing point or the empty string if point
isn't in a reference."

  ; Find the ends of the reference contining point.

  (let ((ends (btxlook-reference-ends)))

    ; If point isn't in a reference, return the empty string.

    (if (not ends) ""

      ; Otherwise extract and return the reference's key.

      (let ((stop (cdr ends)))
	(goto-char (car ends))
	(or (re-search-forward "^@" stop t)
	    (error "Missing \"@\" in reference"))
	(or (re-search-forward "[^{]*{[ \t\n]*\\([^ \t\n,]*\\)[ \t\n,]" stop t)
	    (error "Missing key in reference"))
	(buffer-substring (match-beginning 1) (match-end 1))
      )
    )
  )
)


(defun btxlook-get-search-keywords ()

  "Prompt for a list of search keywords and return them."

  (interactive)

  (let* ((minibuffer-history-position 1)
	 (minibuffer-history-sexp-flag nil)
	 (minibuffer-history-variable 'btxlook-search-command-history)
	 (keywords (read-from-minibuffer
                  "Search for:  " "" read-expression-map nil
                  'btxlook-search-command-history))
	)

    ; If no keywords were given, delete the empty entry from the history list.

    (if (= 0 (length keywords))
      (setq btxlook-search-command-history
        (cdr btxlook-search-command-history)))

    keywords
  )
)


(defun btxlook-help ()

  "Show the btxlook mode help."

  (interactive)
  (describe-mode)
)


(defun btxlook-insert-citations (keys)

  "Insert citations for KEYS at point in the current buffer."

  (if (> (length keys) 0)
    (let ((keys (mapconcat '(lambda (x) x) keys ",")))
      (insert "\\" btxlook-citation-cs "{" keys "}"))
  )
)


(defun btxlook-insert-reference ()

"Insert the reference containing point and all marked references as citations
and return to the previous buffer."

  (interactive)

  (let ((ends (btxlook-reference-ends)))
    (if (not ends) (message "The cursor is not in a reference.")
      (goto-char (car ends))
      (if (looking-at "\\* /") nil
	(setq buffer-read-only nil)
	(insert "* ")
      )
      (btxlook-exit)
    )
  )
)


(defun btxlook-make-mode-line ()

  "Set the match buffer mode line."

  (let* ((ec (btxlook-count-references))
	 (f (car ec))
	)

    (setq mode-line-format 
      (format " Match %d of %d for %s" f (+ f (cdr ec)) btxlook-command))
  )
)


(defun btxlook-mode () 

"Prompt for a list of words and display a buffer of references containing the
words. While in the match buffer, the following commands are available (the
current reference is the reference containing the cursor):

  \\[btxlook-scroll-up]  scroll the buffer up one page.
  \\[btxlook-exit]    insert all marked references as citations and return.
  \\[btxlook-help]    show this help information.
  \\[btxlook-insert-reference]    insert citations for the current and marked references and return.
  \\[btxlook-mark-reference]    mark or unmark the current reference.
  \\[btxlook-next-reference]    move to the reference following the current reference.
  \\[btxlook-previous-reference]    move to the reference preceeding the current reference.
  \\[btxlook-quit]    return to the previous buffer, making no changes.
  \\[btxlook-research]    prompt for another list of words and search again.
  \\[btxlook-update]    check for, and update, out-of-date bix files.
"

  (interactive)

  (save-excursion
    (if (not btxlook-process) (btxlook-start-process btxlook-process-buffer))
    (set-buffer btxlook-process-buffer)
    (kill-all-local-variables)

    (setq major-mode 'btxlook-mode)
    (setq mode-name "btxlook")
  
    (use-local-map btxlook-mode-map)
    (put 'btxlook-mode 'mode-class 'special)

    (require 'comint)
  )
)


(defun btxlook-mark-reference ()

  "Toggle the mark on the current reference."

  (interactive)
  (let ((ends (btxlook-reference-ends)))
    (if (not ends) (message "The cursor is not in a reference.")
      (save-excursion
	(goto-char (car ends))
	(setq buffer-read-only nil)
	(if (looking-at "* ") (delete-char 2) (insert "* "))
	(setq buffer-read-only t)
      )
    )
  )
)


(defun btxlook-next-reference ()

  "Move point to the start of the next reference"

  (interactive)
  (if (not (search-forward-regexp "^\\(\\* \\)?/.*\n@" (point-max) t 1)) nil
    (beginning-of-line)
    (recenter 1)
    (btxlook-make-mode-line)
  )
)


(defun btxlook-previous-reference ()

  "Move point to the start of the previous reference"

  (interactive)
  (if (not (search-backward-regexp "^\\(\\* \\)?/.*\n@" (point-min) t 1)) nil
    (beginning-of-line 2)
    (recenter 1)
    (btxlook-make-mode-line)
  )
)


(defun btxlook-reference-ends ()

"Return a cons cell giving the ends of the reference containing point or nil if
point isn't in a reference.  The car of the cell is the end closest to the
start of the buffer."

  (save-excursion
    (if (looking-at "\\(\n+\\(\\* \\)?/\\)\\|\\(\n*\\'\\)") nil
      (let (start end)
	(setq start 
	  (cond ((looking-at "^\\(\\* \\)?/") (point))
		((re-search-backward "\n\\(\\* \\)?/" (point-min) t)
		   (1+ (point)))
		((re-search-backward "\\`\\(\\* \\)?/" (point-min) t)
		   (point-min))
		(t (error "Missing reference start"))
	  ))
	(if (re-search-forward "}\n\n" (point-max) t)
	  (cons start (match-beginning 0))
	  (error "Missing reference end"))
      )
    )
  )  
)

(defun btxlook-scroll-up ()

  "Scroll the buffer up a page."

  (interactive)
  (scroll-up)
  (btxlook-make-mode-line)
)


(defun btxlook-quit ()

"Return to the previous buffer without making any changes."

  (interactive)

  (switch-to-buffer btxlook-previous-buffer)
)


(defun btxlook-research ()

"Prompt for a list of search keywords and submit them to the btxlook process.
Any matches replace the current contents of the buffer."

  (interactive)

  (let ((words (btxlook-get-search-keywords)))
    (if (> (length words) 0) (btxlook-do-search words))
  )
)


(defun btxlook-search (insert-override)

"Prompt for a list of search keywords, submit them to btxlook, and switch to a
buffer displaying the matches.  If INSERT-OVERRIDE is 1 (the default),
btxlook-insert-single-match controls whether a single reference search is
automatically inserted as a citation; if INSERT-OVERRIDE is not equal to 1,
btxlook-insert-single-match is ignored and a single reference search is not
automatically inserted as a citation."

  (interactive "p")

  (let ((words (btxlook-get-search-keywords)))
    (if (= (length words) 0) nil
      (setq btxlook-previous-buffer (current-buffer))
      (set-buffer btxlook-process-buffer)

      (btxlook-do-search words)

      ; See how many references matched.

      (let* ((cnts (btxlook-count-references))
	     (c (+ (car cnts) (cdr cnts)))
	    )
	(cond ((= c 0)

		; No matches.  Rather than show the empty buffer, print a
		; message indicating no match.  There are two reasons for not
		; getting a match.

		(message

		  ; If the buffer's empty, no reference contained all the
		  ; words.

		  (if (= (point-max) 1) "No matches found."

		    ; Otherwise there were words not found in any reference.
		    ; Btxlook identified the errant words in its response;
		    ; remove trailing newlines from the response and show it.

		    (let ((end (1- (point-max))))
		      (goto-char end)
		      (while (looking-at "\n")
			(setq end (1- end)) (backward-char 1))
		      (buffer-substring (point-min) (1+ end))
		    )
		  )
		))

	      ((and (= c 1) (= insert-override 1) btxlook-insert-single-match)

		 ; A single match to be inserted automatically.

		 (btxlook-insert-reference))

	      (t
		 ; Show the match buffer.

		 (switch-to-buffer btxlook-process-buffer))
	    )
       )
    )
  )
)


(defun btxlook-start-process (b)

  "Start a btxlook process having B as its process buffer."

  (setq btxlook-process
    (let ((process-connection-type nil))
      (start-process
	"btxlook" b "btxlook" "-d" "cat")))
  (process-kill-without-query btxlook-process)
)


(defun btxlook-update ()

"Check for out-of-date bix files and update any found.  Do this by killing the
 btxlook process and restarting it.  Crude but effective."

  (interactive)

  (if (not btxlook-process) nil
    (set-process-buffer btxlook-process nil)
    (process-send-eof btxlook-process)
  )

  (let ((b (get-buffer-create "*btxlook-junk*")))
    (message "Restarting btxlook process...")
    (btxlook-start-process b)

    (message "Waiting for btxlook process output...")
    (set-buffer b)
    (btxlook-waitfor-prompt)
    (if (> (point-max) 0)
      (with-output-to-temp-buffer "btxlook update error"
	(princ (concat "Error during update:\n\n" (buffer-string)))))

    (set-buffer btxlook-process-buffer)
    (message "Done.")
    (set-process-buffer btxlook-process btxlook-process-buffer)
    (kill-buffer b)
  )

)


(defun btxlook-waitfor-prompt ()

"Wait for the btxlook process to return its prompt, delete the prompt, and
return."

  (let (more)

    ; Return nil if the btxlook process has sent a prompt to the buffer, t
    ; otherwise.  If the prompt has been received, delete it.

    (fset 'more
      (lambda () 
	(if (< (buffer-size) 2) t
	  (goto-char (- (point-max) 2))
	  (if (not (looking-at ": ")) t
	    (delete-char 2)
	    nil
	  )))
    )

    (while (more) (accept-process-output btxlook-process))
  )
)


(provide 'btxlook)
