# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

# Posts to Discord what upstream changed in the dependencies a repository pins.
# For OxC3 those are the forks in packages/*/conandata.yml, the Agility SDK's NuGet packages and wayland-protocols.
# Another repository is read with -root and -conandata; a pin that is no fork is compared against its own default branch.
# Only what moved since the last report is posted.
#
#   python tools/deps_watch.py                    post what moved since the state file, then update it
#   python tools/deps_watch.py -dry               print it instead, leaving the state as it was
#   python tools/deps_watch.py -all               every dependency that is behind upstream, moved or not
#   python tools/deps_watch.py -state <file>      the last report's view of upstream (build/deps_watch.json in the root)
#   python tools/deps_watch.py -root <dir>        the repository whose pins are read (this checkout of OxC3)
#   python tools/deps_watch.py -conandata <glob>  its conandata.yml files that pin git (packages/*/conandata.yml)
#   python tools/deps_watch.py -branch <id>=<b>   follow upstream branch b for dependency id, not the default branch
#
# WEBHOOK_URL is where it posts; without it the report is printed and the state kept.
# GH_TOKEN lifts GitHub's limit of 60 anonymous requests an hour.
#
# A report says how far the pin is behind and lists the new commits and release.
# Vulkan-Headers' also lists the extensions and SPIR-V capabilities its registry gained.

import argparse
import glob
import http.client
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GITHUB = "https://api.github.com"

# Discord's limits: 10 embeds and 6000 characters of them a message, 4096 a description, 256 a title.

EMBEDS_PER_MESSAGE = 10
CHARS_PER_MESSAGE = 6000
CHARS_PER_DESCRIPTION = 4096
COMMIT_LINES = 8

COLOR_MOVED = 3447003
COLOR_FAILED = 15158332

# A GET, retried with a growing wait: a large compare is sometimes cut off mid response.
# Of the HTTP errors only a rate limit or a server error is worth asking again; the rest are answers.

RETRY_STATUS = (429, 500, 502, 503, 504)

def fetch(url, token = None, raw = False):

	headers = { "User-Agent": "oxc3-deps-watch" }

	if url.startswith(GITHUB):
		headers["Accept"] = "application/vnd.github+json"

		if token:
			headers["Authorization"] = "Bearer " + token

	for attempt in range(3):

		try:
			with urllib.request.urlopen(urllib.request.Request(url, headers = headers), timeout = 120) as response:
				data = response.read()

			break

		except urllib.error.HTTPError as e:

			if e.code not in RETRY_STATUS or attempt == 2:
				raise

		except (OSError, http.client.HTTPException):

			if attempt == 2:
				raise

			time.sleep(5 * (attempt + 1))

	return data.decode("utf-8") if raw else json.loads(data)

# The pins, as the repository's own files state them.

# A dependency is named after the directory its conandata.yml sits in.
# The NuGet and wayland-protocols pins are OxC3's packages, so a root without those files has none.

def gitPins(root, pattern):

	pins = []

	for path in sorted(glob.glob(os.path.join(root, pattern))):

		with open(path) as f:
			text = f.read()

		repo = re.search(r'url:\s*"https://github\.com/([^/"]+/[^/"]+?)(?:\.git)?"', text)
		ref = re.search(r'checkout:\s*"([^"]+)"', text)

		if repo and ref:
			pins.append({
				"kind": "git", "id": os.path.basename(os.path.dirname(path)), "repo": repo.group(1), "ref": ref.group(1),
				"home": f"https://github.com/{repo.group(1)}"
			})

	return pins

def nugetPins(root):

	path = os.path.join(root, "packages", "agility_sdk", "conanfile.py")

	if not os.path.isfile(path):
		return []

	with open(path) as f:
		text = f.read()

	return [
		{ "kind": "nuget", "id": package, "pin": version, "home": f"https://www.nuget.org/packages/{package}" }
		for package, version in re.findall(r'nuget\.org/api/v2/package/([^/"]+)/([^/"]+)"', text)
	]

def waylandPins(root):

	path = os.path.join(root, "packages", "xdg_shell", "conanfile.py")

	if not os.path.isfile(path):
		return []

	with open(path) as f:
		match = re.search(r'wayland-protocols/-/raw/([0-9.]+)/', f.read())

	if not match:
		return []

	return [{
		"kind": "gitlab", "id": "wayland-protocols", "project": "wayland/wayland-protocols", "pin": match.group(1),
		"home": "https://gitlab.freedesktop.org/wayland/wayland-protocols"
	}]

# Where each dependency's upstream is now.

def watchGit(dep, token):

	# A repository that is no fork is its own upstream, with no fork side to report.

	pinned = fetch(f"{GITHUB}/repos/{dep['repo']}", token)
	isFork = "parent" in pinned
	parent = pinned["parent"] if isFork else pinned

	# A checkout is a commit, a tag or a branch, the last of which is pinned to whatever it points at today.

	pin = dep["ref"]

	if not re.fullmatch(r"[0-9a-f]{40}", pin):
		pin = fetch(f"{GITHUB}/repos/{dep['repo']}/commits/{urllib.parse.quote(pin, safe = '')}", token)["sha"]

	upstream, forkBranch = parent["full_name"], pinned["default_branch"]
	branch = dep.get("branch") or parent["default_branch"]

	head = fetch(f"{GITHUB}/repos/{upstream}/commits/{branch}", token)["sha"]
	forkHead = fetch(f"{GITHUB}/repos/{dep['repo']}/commits/{forkBranch}", token)["sha"]

	# Asked of the fork, with upstream's branch named by owner, since the pin may be a commit only the fork has.
	# ahead_by counts upstream's commits the pin lacks and behind_by the pin's own.
	# The commit list holds the former, oldest first and at most 250 of them.

	owner = upstream.split("/")[0]
	compare = fetch(f"{GITHUB}/repos/{dep['repo']}/compare/{pin}...{owner}:{branch}", token)

	forkAhead = 0

	if isFork and forkHead != pin:
		forkAhead = fetch(f"{GITHUB}/repos/{dep['repo']}/compare/{pin}...{forkHead}", token)["ahead_by"]

	try:
		release = fetch(f"{GITHUB}/repos/{upstream}/releases/latest", token).get("tag_name", "")

	except urllib.error.HTTPError as e:

		if e.code != 404:
			raise

		release = ""

	return {
		"ref": dep["ref"], "pin": pin, "upstream": upstream, "branch": branch, "head": head,
		"fork": dep["repo"], "forkBranch": forkBranch, "forkHead": forkHead, "forkAhead": forkAhead,
		"behind": compare["ahead_by"], "own": compare["behind_by"], "compare": compare["html_url"],
		"commits": [[c["sha"], c["commit"]["message"].split("\n")[0]] for c in compare["commits"]],
		"release": release
	}

def watchNuget(dep):

	versions = fetch(f"https://api.nuget.org/v3-flatcontainer/{dep['id'].lower()}/index.json")["versions"]
	pin = dep["pin"].lower()
	newer = versions[versions.index(pin) + 1:] if pin in versions else versions[-3:]

	return { "pin": dep["pin"], "latest": versions[-1], "newer": newer }

def versionKey(version):
	return tuple(int(x) for x in re.findall(r"\d+", version))

def watchGitlab(dep):

	project = urllib.parse.quote(dep["project"], safe = "")
	tags = fetch(f"https://gitlab.freedesktop.org/api/v4/projects/{project}/repository/tags?per_page=100")

	newer = sorted(
		(t["name"] for t in tags if re.fullmatch(r"[0-9.]+", t["name"]) and versionKey(t["name"]) > versionKey(dep["pin"])),
		key = versionKey
	)

	return { "pin": dep["pin"], "latest": newer[-1] if newer else dep["pin"], "newer": newer }

# What a Vulkan registry names: its Vulkan extensions, SPIR-V capabilities and SPIR-V extensions.

def registryNames(xml):

	extensions = {
		name for name, supported in re.findall(r'<extension name="(VK_\w+)"[^>]*?supported="([^"]*)"', xml)
		if "vulkan" in supported.split(",")
	}

	capabilities = set(re.findall(r'<spirvcapability name="(\w+)"', xml))
	spirvExtensions = set(re.findall(r'<spirvextension name="(\w+)"', xml))
	return extensions, capabilities, spirvExtensions

def registryAt(repository, commit, token):
	return registryNames(fetch(f"https://raw.githubusercontent.com/{repository}/{commit}/registry/vk.xml", token, True))

def registryGained(now, token):

	before = registryAt(now["fork"], now["pin"], token)
	after = registryAt(now["upstream"], now["head"], token)

	return [sorted(b - a) for a, b in zip(before, after)]

# A dependency's report: an embed, or None when there is nothing to say.

def clip(text, length):
	return text if len(text) <= length else text[:length - 3] + "..."

def namesLine(label, names):
	shown = ", ".join(f"`{n}`" for n in names[:12])
	return f"{len(names)} {label}: {shown}" + (f" and {len(names) - 12} more" if len(names) > 12 else "")

def describeGit(dep, now, before, everything, token):

	moved = (
		not before or now["head"] != before.get("head") or now["forkHead"] != before.get("forkHead") or
		now["release"] != before.get("release")
	)

	if not (moved or everything) or (not now["behind"] and not now["forkAhead"]):
		return None

	lines = []
	shas = [c[0] for c in now["commits"]]

	if before and before.get("head") in shas:
		new = now["commits"][shas.index(before["head"]) + 1:]
		lines.append(f"{now['upstream']} `{now['branch']}` added {len(new)} commits since the last report.")

	else:
		new = now["commits"][-COMMIT_LINES:]

	named = f"`{now['ref']}` (`{now['pin'][:8]}`)" if now["ref"] != now["pin"] else f"`{now['pin'][:8]}`"
	pinned = f"The pin {named} is {now['behind']} commits behind `{now['branch']}`"
	lines.append(pinned + (f", with {now['own']} of its own on top." if now["own"] else "."))

	for sha, subject in new[-COMMIT_LINES:]:
		lines.append(f"`{sha[:7]}` {clip(subject, 100)}")

	if len(new) > COMMIT_LINES:
		lines.append(f"... and {len(new) - COMMIT_LINES} earlier ones")

	if now["release"] and (not before or now["release"] != before.get("release")):
		lines.append(f"Latest release: `{now['release']}`")

	if now["forkAhead"]:
		lines.append(f"{now['fork']} `{now['forkBranch']}` is {now['forkAhead']} commits past the pin.")

	if dep["id"] == "vulkan_headers" and now["behind"]:

		extensions, capabilities, spirvExtensions = registryGained(now, token)
		gained = [
			namesLine("Vulkan extensions", extensions) if extensions else "",
			namesLine("SPIR-V capabilities", capabilities) if capabilities else "",
			namesLine("SPIR-V extensions", spirvExtensions) if spirvExtensions else ""
		]

		if any(gained):
			lines.append("Gained since the pin: " + "; ".join(g for g in gained if g))

	return {
		"title": clip(f"{dep['id']}: {now['upstream']}", 256), "url": now["compare"],
		"description": clip("\n".join(lines), CHARS_PER_DESCRIPTION), "color": COLOR_MOVED
	}

def describeVersions(dep, now, before, everything, url):

	moved = not before or now["latest"] != before.get("latest")

	if not (moved or everything) or not now["newer"]:
		return None

	lines = [f"Pinned `{now['pin']}`, latest `{now['latest']}`."]
	lines.append("Newer: " + ", ".join(f"`{v}`" for v in now["newer"][-12:]))

	return {
		"title": clip(dep["id"], 256), "url": url,
		"description": clip("\n".join(lines), CHARS_PER_DESCRIPTION), "color": COLOR_MOVED
	}

# Posting: embeds packed into as few messages as Discord's limits allow.

def embedChars(embed):
	return len(embed.get("title", "")) + len(embed.get("description", ""))

def post(webhook, embeds):

	messages, current, chars = [], [], 0

	for embed in embeds:

		if current and (len(current) == EMBEDS_PER_MESSAGE or chars + embedChars(embed) > CHARS_PER_MESSAGE):
			messages.append(current)
			current, chars = [], 0

		current.append(embed)
		chars += embedChars(embed)

	if current:
		messages.append(current)

	# Returns how many embeds went out, since the state records only those.

	sent = 0

	for message in messages:

		body = json.dumps({ "embeds": message }).encode("utf-8")
		delivered = False

		for attempt in range(5):

			request = urllib.request.Request(
				webhook, data = body, headers = { "Content-Type": "application/json", "User-Agent": "oxc3-deps-watch" }
			)

			try:
				urllib.request.urlopen(request, timeout = 60).close()
				delivered = True
				break

			# Discord answers a burst with 429 and how long to wait, which is trusted up to a minute.
			# Any other failure ends the post.

			except urllib.error.HTTPError as e:

				if e.code != 429 or attempt == 4:
					print(f"[deps_watch] the post was refused: {e}", flush = True)
					break

				try:
					wait = float(json.loads(e.read() or b"{}").get("retry_after", 2))

				except (ValueError, AttributeError):
					wait = 2

				time.sleep(min(max(wait, 0), 60))

			except OSError as e:
				print(f"[deps_watch] the post failed: {e}", flush = True)
				break

		if not delivered:
			return sent

		sent += len(message)

	return sent

def summary(embeds):
	return "\n\n".join(f"### [{e['title']}]({e['url']})\n" + e["description"] for e in embeds)

def branchArg(text):

	if "=" not in text or text.startswith("="):
		raise argparse.ArgumentTypeError(f"'{text}' should be ID=BRANCH")

	return text

def main():

	parser = argparse.ArgumentParser(description = "What upstream did to the dependencies a repository pins")
	parser.add_argument("-root", default = ROOT, help = "the repository whose pins are read")
	parser.add_argument("-conandata", default = "packages/*/conandata.yml", help = "its conandata.yml files that pin git")
	parser.add_argument("-state", help = "the last report's view of upstream, build/deps_watch.json in the root")
	parser.add_argument(
		"-branch", action = "append", default = [], metavar = "ID=BRANCH", type = branchArg,
		help = "follow this upstream branch for a dependency instead of its default branch"
	)
	parser.add_argument("-dry", action = "store_true", help = "print the report, leave the state alone")
	parser.add_argument("-all", action = "store_true", help = "report everything behind upstream, moved or not")
	args = parser.parse_args()

	if not args.state:
		args.state = os.path.join(args.root, "build", "deps_watch.json")

	branches = dict(b.split("=", 1) for b in args.branch)

	token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
	webhook = os.environ.get("WEBHOOK_URL")

	# A state that can't be read costs one full report rather than every later night.

	state = {}

	if os.path.isfile(args.state):

		try:
			with open(args.state) as f:
				state = json.load(f)

		except (OSError, ValueError) as e:
			print(f"[deps_watch] ignoring the unreadable state {args.state}: {e}", flush = True)

	deps = state.get("deps", {})
	nextDeps, embeds, embedKeys, failed = dict(deps), [], [], 0

	for dep in gitPins(args.root, args.conandata) + nugetPins(args.root) + waylandPins(args.root):

		dep["branch"] = branches.get(dep["id"])
		key = f"{dep['kind']}:{dep['id']}"
		before = deps.get(key)

		# A query that fails is reported and leaves its state as it was, so the change it missed comes next time.
		# Anything it raises counts: a response cut short is as much a failed query as a refused one.

		try:

			if dep["kind"] == "git":
				now = watchGit(dep, token)
				embed = describeGit(dep, now, before, args.all, token)
				del now["commits"]

			elif dep["kind"] == "nuget":
				now = watchNuget(dep)
				embed = describeVersions(
					dep, now, before, args.all, f"https://www.nuget.org/packages/{dep['id']}/{now['latest']}"
				)

			else:
				now = watchGitlab(dep)
				embed = describeVersions(
					dep, now, before, args.all, f"https://gitlab.freedesktop.org/{dep['project']}/-/tags/{now['latest']}"
				)

			nextDeps[key] = now

		except Exception as e:

			failed += 1
			embed = {
				"title": clip(f"{dep['id']}: query failed", 256), "url": dep["home"],
				"description": clip(f"{type(e).__name__}: {e}", CHARS_PER_DESCRIPTION), "color": COLOR_FAILED
			}

		if embed:
			embeds.append(embed)
			embedKeys.append(key)

		status = "query failed" if embed and embed["color"] == COLOR_FAILED else "moved" if embed else "nothing new"
		print(f"[deps_watch] {key}: {status}", flush = True)

	report = summary(embeds) if embeds else "Nothing moved since the last report."

	if os.environ.get("GITHUB_STEP_SUMMARY"):
		with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as f:
			f.write("## Dependency watch\n\n" + report + "\n")

	# A failed query is reported and the run still fails, so the job shows it.

	if args.dry or not webhook:
		print(report)
		return 1 if failed else 0

	sent = post(webhook, embeds) if embeds else 0

	# A report that did not go out leaves its dependency's state as it was, so the next run sends it again.

	for key in embedKeys[sent:]:

		if key in deps:
			nextDeps[key] = deps[key]

		else:
			nextDeps.pop(key, None)

	failed += len(embeds) - sent

	# Written beside and moved over, so a run cut short leaves the last state whole.

	os.makedirs(os.path.dirname(os.path.abspath(args.state)), exist_ok = True)

	with open(args.state + ".tmp", "w") as f:
		json.dump({ "version": 1, "deps": nextDeps }, f, indent = "\t")

	os.replace(args.state + ".tmp", args.state)
	return 1 if failed else 0

if __name__ == "__main__":
	sys.exit(main())
