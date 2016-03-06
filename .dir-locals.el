(
 (c++-mode . (
              (eval . (progn
                        (setq flycheck-clang-include-path (list (expand-file-name "./external/include/")))
                        (flycheck-select-checker 'c/c++-clang)
                        )
                    )
              )
           )

 )
