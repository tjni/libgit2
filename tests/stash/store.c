#include "clar_libgit2.h"
#include "futils.h"
#include "stash_helpers.h"

static git_repository *repo;
static git_signature *signature;
static git_oid stash_tip_oid;

/*
 * Friendly reminder, in order to ease the reading of the following tests:
 *
 * "stash"		points to the worktree commit
 * "stash^1"	points to the base commit (HEAD when the stash was created)
 * "stash^2"	points to the index commit
 * "stash^3"	points to the untracked commit
 */

void test_stash_store__initialize(void)
{
	cl_git_pass(git_repository_init(&repo, "stash", 0));
	cl_git_pass(git_signature_new(&signature, "nulltoken", "emeric.fermas@gmail.com", 1323847743, 60)); /* Wed Dec 14 08:29:03 2011 +0100 */

	setup_stash(repo, signature);
}

void test_stash_store__cleanup(void)
{
	git_signature_free(signature);
	signature = NULL;

	git_repository_free(repo);
	repo = NULL;

	cl_git_pass(git_futils_rmdir_r("stash", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_fixture_cleanup("sorry-it-is-a-non-bare-only-party");
}

static void assert_blob_oid(const char* revision, const char* expected_oid)
{
	assert_object_oid(repo, revision, expected_oid, GIT_OBJECT_BLOB);
}

void test_stash_store__saves_stash_without_updating_workdir(void)
{
	/* Asserts expected initial status. */

	assert_status(repo, "what", GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "who", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "why", GIT_STATUS_INDEX_NEW);
	assert_status(repo, "where", GIT_STATUS_INDEX_NEW | GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);

	cl_git_pass(git_stash_create(&stash_tip_oid, repo, signature, NULL, GIT_STASH_DEFAULT));
	cl_git_pass(git_stash_store(&stash_tip_oid, repo, NULL));

	/* Tests that the stash commit is created successfully. */

	assert_object_oid(repo, "refs/stash", git_oid_tostr_s(&stash_tip_oid), GIT_OBJECT_COMMIT);

	assert_blob_oid("refs/stash:what", "bc99dc98b3eba0e9157e94769cd4d49cb49de449");	/* see you later */
	assert_blob_oid("refs/stash:how", "e6d64adb2c7f3eb8feb493b556cc8070dca379a3");	/* not so small and */
	assert_blob_oid("refs/stash:who", "a0400d4954659306a976567af43125a0b1aa8595");	/* funky world */
	assert_blob_oid("refs/stash:when", NULL);
	assert_blob_oid("refs/stash:why", "88c2533e21f098b89c91a431d8075cbdbe422a51"); /* would anybody use stash? */
	assert_blob_oid("refs/stash:where", "e3d6434ec12eb76af8dfa843a64ba6ab91014a0b"); /* .... */
	assert_blob_oid("refs/stash:.gitignore", "ac4d88de61733173d9959e4b77c69b9f17a00980");
	assert_blob_oid("refs/stash:just.ignore", NULL);

	assert_blob_oid("refs/stash^2:what", "dd7e1c6f0fefe118f0b63d9f10908c460aa317a6");	/* goodbye */
	assert_blob_oid("refs/stash^2:how", "e6d64adb2c7f3eb8feb493b556cc8070dca379a3");	/* not so small and */
	assert_blob_oid("refs/stash^2:who", "cc628ccd10742baea8241c5924df992b5c019f71");	/* world */
	assert_blob_oid("refs/stash^2:when", NULL);
	assert_blob_oid("refs/stash^2:why", "88c2533e21f098b89c91a431d8075cbdbe422a51"); /* would anybody use stash? */
	assert_blob_oid("refs/stash^2:where", "e08f7fbb9a42a0c5367cf8b349f1f08c3d56bd72"); /* ???? */
	assert_blob_oid("refs/stash^2:.gitignore", "ac4d88de61733173d9959e4b77c69b9f17a00980");
	assert_blob_oid("refs/stash^2:just.ignore", NULL);

	assert_blob_oid("refs/stash^3", NULL);

	/* Tests that the working directory and index have not changed. */

	assert_status(repo, "what", GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "how", GIT_STATUS_INDEX_MODIFIED);
	assert_status(repo, "who", GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "why", GIT_STATUS_INDEX_NEW);
	assert_status(repo, "where", GIT_STATUS_INDEX_NEW | GIT_STATUS_WT_MODIFIED);
	assert_status(repo, "when", GIT_STATUS_WT_NEW);
}

void test_stash_store__updates_the_reflog(void)
{
	assert_object_oid(repo, "refs/stash@{0}", NULL, GIT_OBJECT_COMMIT);

	cl_git_pass(git_stash_create(&stash_tip_oid, repo, signature, NULL, GIT_STASH_DEFAULT));
	cl_git_pass(git_stash_store(&stash_tip_oid, repo, NULL));

	assert_object_oid(repo, "refs/stash@{0}", git_oid_tostr_s(&stash_tip_oid), GIT_OBJECT_COMMIT);
	assert_object_oid(repo, "refs/stash@{1}", NULL, GIT_OBJECT_COMMIT);
}

void test_stash_store__saves_a_default_stash_message(void)
{
	const git_reflog_entry *entry;
	git_reflog *reflog;

	cl_git_pass(git_stash_create(&stash_tip_oid, repo, signature, NULL, GIT_STASH_DEFAULT));
	cl_git_pass(git_stash_store(&stash_tip_oid, repo, NULL));

	cl_git_pass(git_reflog_read(&reflog, repo, "refs/stash"));
	cl_assert(entry = git_reflog_entry_byindex(reflog, 0));
	cl_assert_equal_s(git_reflog_entry_message(entry), "Created via \"git stash store\".");

	git_reflog_free(reflog);
}

#define CUSTOM_STORE_MESSAGE "Look Ma! I'm on TV!"
void test_stash_store__can_save_a_custom_stash_message(void)
{
	const git_reflog_entry *entry;
	git_reflog *reflog;

	cl_git_pass(git_stash_create(&stash_tip_oid, repo, signature, NULL, GIT_STASH_DEFAULT));
	cl_git_pass(git_stash_store(&stash_tip_oid, repo, CUSTOM_STORE_MESSAGE));

	cl_git_pass(git_reflog_read(&reflog, repo, "refs/stash"));
	cl_assert(entry = git_reflog_entry_byindex(reflog, 0));
	cl_assert_equal_s(git_reflog_entry_message(entry), CUSTOM_STORE_MESSAGE);

	git_reflog_free(reflog);
}
