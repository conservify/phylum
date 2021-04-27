@Library('conservify') _

conservifyProperties()

timestamps {
    try {
		node ("jenkins-aws-ubuntu") {
			def scmInfo

			stage ('git') {
				scmInfo = checkout scm
			}

			def (remote, branch) = scmInfo.GIT_BRANCH.tokenize('/')

			stage ('clean') {
				sh "make clean"
			}

			stage ('build') {
				sh "make"
			}

			def version = readFile('build/version.txt')

			currentBuild.description = version.trim()

			stage ('build') {
				sh "make test"
			}
		}
        notifySuccess()
    }
    catch (Exception e) {
        notifyFailure()
        throw e;
    }
}
