;; Emacs settings for this repo (common/mulhearn/code_style.txt):  wrap at 100 columns (default 70;  M-q,
;; auto-fill), spaces only, two spaces after a sentence;  C and C++ indented by 2.
((nil . ((fill-column . 100)
         (indent-tabs-mode . nil)
         (sentence-end-double-space . t)))
 (c-mode . ((c-basic-offset . 2)))
 (c++-mode . ((c-basic-offset . 2))))
