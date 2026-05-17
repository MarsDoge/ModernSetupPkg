# Brand Assets

`logo.jpg` is a project-provided visual reference asset for README and
showcase material. It is not included in the ArmVirt firmware image and is not
referenced by the default DSC/FDF path.

The current firmware build has very limited FVMAIN free space, so product logo
rendering should use a small generated GOP primitive or a compact indexed/RLE
bitmap only after the image pipeline and license boundary are finalized.
