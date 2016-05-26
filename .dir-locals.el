;;; Directory Local Variables
;;; For more information see (info "(emacs) Directory Variables")

((nil)
 (c++-mode
  (flycheck-gcc-language-standard . "c++11")
  (flycheck-clang-language-standard . "c++11")
  (eval progn
        (setq flycheck-clang-include-path
              (list
               (expand-file-name "./external/include/")))
        (flycheck-select-checker 'c/c++-clang))))
