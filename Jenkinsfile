@Library('conservify') _

conservifyProperties()

timestamps {
    node ("jenkins-aws-ubuntu") {
        conservifyBuild(name: 'phylum', test: true)
    }
}
