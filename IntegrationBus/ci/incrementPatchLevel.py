import re
import subprocess
import sys
import urllib.parse

COMMIT_MESSAGE = "-- managed-build bump version number to "

# check if last commit was generated by this script
last_commit_message = subprocess.getoutput("git log -1 --pretty=%s")
if "-- managed-build" in last_commit_message:
    print("change was triggered by nightly build - aborting.");
    sys.exit(2)

# generate changelog string
changelogRaw = subprocess.getoutput("git log -20 --pretty=%s")
changelogLines = changelogRaw.splitlines()
changelog = "<ul>"
for line in changelogLines:
    if not ("-- managed-build" in line):
        if len(line) > 5:
            changelog += "<li>"+str(line)+"\n</li>"
    else:
        break
changelog += "</ul>"
# write new version number to text file
with open('../../changelog.txt', 'w') as the_file:
    the_file.write(changelog)


# get current branch name
branch_name = subprocess.getoutput("git branch --show-current")

# get git remote url
git_remote_url = subprocess.getoutput("git config --get remote.origin.url")

major = 0
minor = 0
patch = 0

# determine current version number and increment it
with open('../cmake/IntegrationBusVersion.cmake','r') as f:
    newlines = []
    for line in f.readlines():
        if("set(IB_VERSION_MAJOR" in line):
            last_space = line.rindex(" ")
            major = int(re.sub("[^0-9]", "", line[last_space+1:]))
        if("set(IB_VERSION_MINOR" in line):
            last_space = line.rindex(" ")
            minor = int(re.sub("[^0-9]", "", line[last_space+1:]))
        if("set(IB_VERSION_PATCH" in line):
            last_space = line.rindex(" ")
            keep_line = line[0:last_space]
            patch = int(re.sub("[^0-9]", "", line[last_space+1:]))
            patch = patch + 1
            newlines.append(keep_line + " "+ str(patch)+")\n")
        else:
            newlines.append(line)

with open('../cmake/IntegrationBusVersion.cmake', 'w') as f:
    for line in newlines:
        f.write(line)

# push new version number to repository
print("repo url: "+git_remote_url)
print("number of arguments: "+str(len(sys.argv)))
git_remote_url_cred = git_remote_url.replace("https://", "https://"+urllib.parse.quote(sys.argv[1])+":"+urllib.parse.quote(sys.argv[2])+"@");
rc = subprocess.call("git status")
rc = subprocess.call("git add ../cmake/IntegrationBusVersion.cmake")
rc = subprocess.call("git commit -m \""+COMMIT_MESSAGE+str(major)+"."+str(minor)+"."+str(patch)+" (nightly build)\"")
rc = subprocess.call("git remote set-url origin "+git_remote_url_cred)
rc = subprocess.call("git push --set-upstream origin "+branch_name, stdout=subprocess.DEVNULL)
rc = subprocess.call("git remote set-url origin " + git_remote_url)

# write new version number to text file
with open('../../new_version_number.txt', 'w') as the_file:
    the_file.write(str(major)+"."+str(minor)+"."+str(patch))