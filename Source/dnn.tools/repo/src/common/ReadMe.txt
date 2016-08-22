This directory contains libraries shared across projects.
Code which does not have external dependency should go to common_standalone.
Code with dependencies should go to separate project.
This way we avoid need to link unused dependencies.