;;;; -*- Mode: lisp; indent-tabs-mode: nil -*-
;;;
;;; cffi.asd --- ASDF system definition for CFFI.
;;;
;;; Copyright (C) 2005-2006, James Bielman  <jamesjb@jamesjb.com>
;;; Copyright (C) 2005-2010, Luis Oliveira  <loliveira@common-lisp.net>
;;;
;;; Permission is hereby granted, free of charge, to any person
;;; obtaining a copy of this software and associated documentation
;;; files (the "Software"), to deal in the Software without
;;; restriction, including without limitation the rights to use, copy,
;;; modify, merge, publish, distribute, sublicense, and/or sell copies
;;; of the Software, and to permit persons to whom the Software is
;;; furnished to do so, subject to the following conditions:
;;;
;;; The above copyright notice and this permission notice shall be
;;; included in all copies or substantial portions of the Software.
;;;
;;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
;;; EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
;;; MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
;;; NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
;;; HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
;;; WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;;; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
;;; DEALINGS IN THE SOFTWARE.
;;;

(in-package :asdf)

#-(or openmcl mcl sbcl cmucl scl clisp lispworks ecl allegro cormanlisp abcl mkcl)
(error "Sorry, this Lisp is not yet supported.  Patches welcome!")

(defsystem :cffi
  :description "The Common Foreign Function Interface"
  :author "James Bielman  <jamesjb@jamesjb.com>"
  :maintainer "Luis Oliveira  <loliveira@common-lisp.net>"
  :licence "MIT"
  :depends-on (:uiop :alexandria :trivial-features :babel)
  #+:asdf3 :in-order-to #+:asdf3 ((test-op (load-op :cffi-tests)))
  #+:asdf3 :perform #+:asdf3 (test-op (o c) (operate 'asdf:test-op :cffi-tests))
  :components
  ((:module "src"
    :serial t
    :components
    (#+openmcl    (:file "cffi-openmcl")
     #+mcl        (:file "cffi-mcl")
     #+sbcl       (:file "cffi-sbcl")
     #+cmucl      (:file "cffi-cmucl")
     #+scl        (:file "cffi-scl")
     #+clisp      (:file "cffi-clisp")
     #+lispworks  (:file "cffi-lispworks")
     #+ecl        (:file "cffi-ecl")
     #+allegro    (:file "cffi-allegro")
     #+cormanlisp (:file "cffi-corman")
     #+abcl       (:file "cffi-abcl")
     #+mkcl       (:file "cffi-mkcl")
     (:file "package")
     (:file "utils")
     (:file "libraries")
     (:file "early-types")
     (:file "types")
     (:file "enum")
     (:file "strings")
     (:file "structures")
     (:file "functions")
     (:file "foreign-vars")
     (:file "features")))))

;; when you get CFFI from git, its defsystem doesn't have a version,
;; so we assume it satisfies any version requirements whatsoever.
(defmethod version-satisfies ((c (eql (find-system :cffi))) version)
  (declare (ignorable version))
  (or (null (component-version c))
      (call-next-method)))

(defsystem :cffi/c2ffi
  :description "CFFI definition generator from the FFI spec generated by c2ffi. This system is enough to use the ASDF machinery (as a :defsystem-depends-on)."
  :author "Attila Lendvai <attila@lendvai.name>"
  :depends-on (:alexandria
               :asdf
               :cffi
               :uiop)
  :licence "MIT"
  :components
  ((:module "src/c2ffi"
    :components
    ((:file "package")
     (:file "c2ffi" :depends-on ("package"))
     (:file "asdf" :depends-on ("package" "c2ffi"))))))

(defsystem :cffi/c2ffi-generator
  :description "This system gets loaded lazily when the CFFI bindings need to be regenerated."
  :author "Attila Lendvai <attila@lendvai.name>"
  :depends-on (:cffi/c2ffi
               :cl-ppcre
               :cl-json)
  :licence "MIT"
  :components
  ((:module "src/c2ffi"
    :components
    ((:file "generator")))))
