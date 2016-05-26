(
 (nil . (
         (flycheck-clang-language-standard "c++11")
         )
      )
 (c++-mode . (
              (eval . (progn
                        (setq flycheck-clang-include-path (list (expand-file-name "./external/include/")))
                        (flycheck-select-checker 'c/c++-clang)
                       )
                    )
              )
           )

 )
