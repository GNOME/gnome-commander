# Releasing

This file describes how to perform a new release of Gnome Commander using the GNOME infrastructure.

Last update: 2026-11-08

## Preparing a Release

General hints about doing a release can be found in the GNOME release guidelines:
[[https://handbook.gnome.org/maintainers/making-a-release.html]]

## Branching before the next stable release

If a new stable release should be prepared, a new branch must be created some time before the
release. The naming scheme should be `gcmd-X-Y`, where `X` is the major version and `Y` the minor
version of the program.

After creating that new stable branch, update the Gnome Commander project on
[Damned Lies](https://l10n.gnome.org/module/gnome-commander/) to add the new branch and lock the old
one. Only the person mentioned in the [gnome-commander.doap](gnome-commander.doap) file is eligible
for doing that. If in doubt how to login, use the "Legacy Connection" method to login, and use the
email mentioned in the doap-file. You will recieve an email with a login link. (Under some
circumstances this [might fail and require intervention from the infrastructure
team](https://gitlab.gnome.org/Infrastructure/damned-lies/-/work_items/532#note_2159747).)

## Bug fix releases

Bug fix releases should only happen from inside an already existing stable branch. Hence, no new
branch needs to be created. Also, the translation team does not need to be informed unless massive
changes in translatable strings occurred. This is usually not the case for bug fix releases.

## Performing the release

Update the [MetaInfo](data/org.gnome.gnome-commander.metainfo.xml.in) file, adding a new `<release>`
tag with the new features and other changes of the release. Run
`appstream-util validate data/org.gnome.gnome-commander.metainfo.xml.in` and make sure no error is
reported.

Also update [documentation](doc/C/index.docbook), changing version number in the header, adding a
new revision and possibly adjusting the copyright information.

Finally, update the version number in [Cargo.toml](Cargo.toml) file. Make sure that
[Cargo.lock](Cargo.lock) file updates as well, e.g. by running `cargo metadata`. The final commit
should look similar to [this one](https://gitlab.gnome.org/GNOME/gnome-commander/-/commit/13eb5ff98c3800ded3ddf749241f686c5e8ce554).

Push your changes to the respective stable branch. Make sure that CI succeeds. Then create a signed
tag for the release, ideally with a message containing the release notes. This tag will trigger the
`create_tarball` section of the [.gitlab-ci.yml](.gitlab-ci.yml) file on GitLab, uploading a tar.xz
archive to the [GNOME download server](https://download.gnome.org/sources/gnome-commander/). A new
release version will be created automatically in the
[GitLab releases section](https://gitlab.gnome.org/GNOME/gnome-commander/-/releases) with the
release notes taken from the tag message.

The maintainer should edit the release notes to add a link to the new tar.xz-archive on the download
server and a link to the portable build. For the latter, download the build produced for the tag
from [Artifacts](https://gitlab.gnome.org/GNOME/gnome-commander/-/artifacts), then attach the file
to the release notes. Once the upload finishes you can copy the link and remove it from release
notes again, place it into the dedicated links section instead.

Finally, import your stable branch changes into the development branch, except for the `Cargo.toml`
change – for major releases the version on the development branch has to be set higher than the one
on the stable branch, for bugfix releases it stays unchanged. Create a merge request like 
[this one](https://gitlab.gnome.org/GNOME/gnome-commander/-/merge_requests/720).

That’t it.
