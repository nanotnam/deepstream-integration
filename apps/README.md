# Applications

Each child directory is an independently owned and packaged DeepStream application.
An application contains its domain algorithms, runtime orchestration, SDK plugins,
profiles, model contracts, tests, Dockerfile, and operations documentation.

Copy `app-template` when starting a new application. Do not place application-domain
types in `libs/`; promote code only after a second real application needs it.
