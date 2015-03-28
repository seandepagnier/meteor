;;
;; Copyright (C) 2007  Sean D'Epagnier   All Rights Reserved.
;;
;; Meteor is free software; you can redistribute it and/or
;; modify it under the terms of the GNU Library General Public
;; License as published by the Free Software Foundation; either
;; version 3 of the License, or (at your option) any later version.
;;
;; Meteor is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU Library General Public
;; License along with this library; if not, write to the Free
;; Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;;

;; Convert infix strings into prefix expressions
;; Only + - * / operators supported

;; eg: (string-infix->prefix "1*(2+3)*4")
;; Value: (* 1 (+ 2 3) 4)

;; eg: (infix->prefix '(1 + 2 + 3))
;; Value: (+ 1 2 3)

;; Has a repl loop as an infix calculator:
;; #;9> (calc)
;; > 1+2*3+4
;; = 11
;; > quit

;; right now there is a hack to support "-1+1", it would be (+ (- 0 1) 1) because
;; term-optimizer.scm does not support (- 1)

(define (operator? exp)
  (let test ((ops '(+ - * /)))
    (cond ((null? ops) #f)
          ((equal? exp (car ops)) #t)
          (else (test (cdr ops))))))

(define (op-order< x y)
  (and (or (equal? x '+) (equal? x '-))
       (or (equal? y '*) (equal? y '/))))

(define (infix->prefix exp)
  (define (scan-op op args exp)
    (if (null? exp)
        (cons (cons op args) '())
        (let ((x (car exp)))
          (if (operator? x) 
              (if op
                  (cond ((equal? x op) (scan-exp op args (cdr exp)))
                        ((op-order< x op) 
                         (scan-exp x (list (cons op args)) (cdr exp)))
                        ((op-order< op x)
                         (let ((end-arg (car (reverse args)))
                               (body-args (cdr (reverse args))))
                           (let ((val (scan-exp x (list end-arg) (cdr exp))))
                                 (scan-op op (append body-args 
                                                     (if (equal? (caar val) op)
                                                         (cdar val)
                                                         (list (car val))))
                                          (cdr val)))))
                        (else (scan-exp x (list (cons op args)) (cdr exp))))
                  (scan-exp x args (cdr exp)))
              (error "Expected operator")))))
  (define (scan-exp op args exp)
    (let ((x (car exp)))
      (cond ((equal? x '-) (scan-op op (append args (list (list '- 0 (cadr exp)))) (cddr exp)))
            ((pair? x) (scan-op op (append args (list (infix->prefix x))) (cdr exp)))
            ((operator? x) (error "Unexpected operator"))
            (else (scan-op op (append args (list x)) (cdr exp))))))
  (let ((expr (car (scan-exp #f '() exp))))
    (if (car expr) expr (cadr expr))))

;; bonus function
(define (prefix->postfix exp)
  (cond ((null? exp) '())
        ((pair? exp) (append (map prefix->postfix (cdr exp))
                             (list (prefix->postfix (car exp)))))
        (else exp)))

; (+ (* a b) c) => a*b + c
; (+ a (* b c)) => a + b*c

;; takes a string and puts spaces around certain characters
(define (space-op str)
  (list->string
   (let loop1 ((l (string->list str)))
     (define seps (string->list "+-*/()"))
     (if (null? l) '()
         (let loop2 ((s seps))
           (cond ((null? s) (cons (car l) (loop1 (cdr l))))
                 ((equal? (car s) (car l)) (append (list #\space (car l) #\space)
                                                   (loop1 (cdr l))))
                 (else (loop2 (cdr s)))))))))

;; converts expressions like x^2 to (* x x)
(define (remove^2 exp)
    (cond ((pair? exp) (cons (remove^2 (car exp)) (remove^2 (cdr exp))))
          ((symbol? exp)
           (let ((lrstr (reverse (string->list (symbol->string exp)))))
               (if (and (equal? (car lrstr) #\2) (equal? (cadr lrstr) #\^))
                   (let ((x (string->symbol (list->string (reverse (cddr lrstr))))))
                       (list x '* x))
                   exp)))
          (else exp)))

;; take an infix expression as a string and give it in prefix as a symbol
(define (string-infix->prefix str)
  (infix->prefix (remove^2 (read (open-input-string
                                  (string-append "(" (space-op str) ")"))))))

;; calculator mode
(define (calc)
  (display "> ")
  (let ((line (read-line)))
    (cond ((not (equal? "quit" line))
           (display "= ")
           (display (eval (string-infix->prefix line)))
           (newline)
           (calc)))))
