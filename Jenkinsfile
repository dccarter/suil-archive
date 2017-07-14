pipeline {
    agent {
        label "alpine-cpp-build"
    }
    
    parameters {
        string(name: 'VERSION_MAJOR', defaultValue: '0', description: 'Suil major version number')
        string(name: 'VERSION_MINOR', defaultValue: '0', description: 'Suil minor version number')
        string(name: 'VERSION_PATCH', defaultValue: '0', description: 'Suil patch version number')
        string(name: 'VERSION_TAG',   defaultValue: 'devel', description: 'Suil version tag string')
        string(name: 'PROJECT_GROUP_EMAIL', defaultValue: 'lastcarter@gmail.com', description: 'Email address of the group owning the project')
    }

    stages {
        stage('Build') {
            steps {
                sh 'mkdir -p .build'
                dir('.build') {
                    sh 'rm * -rf'
                    sh "cmake .. -DSUIL_BUILD_NUMBER=${env.BUILD_NUMBER} -DSUIL_PATCH_VERSION=${params.VERSION_PATCH} \
                                 -DSUIL_MINOR_VERSION=${params.VERSION_MINOR} -DSUIL_MAJOR_VERSION=${params.VERSION_MAJOR} \
                                 -DSUIL_BUILD_TAG=${params.VERSION_TAG}"
                    sh 'make package -j2'
                }
            }

            post {
                success {
                    archiveArtifacts artifacts: '**/.build/suil-*.tar.gz, \
                                                 **/.build/suil-*.deb, \
                                                 **/.build/sut',
                                    fingerprint: true
                }
            }
        }

        stage('Unit Testing') {
            steps {
                echo 'Starting suil unit tests...'
                dir('.build') {
                    sh './sut -a -r junit > ./sut-results.xml || echo Unit tests failed'
                }
            }

            post {
                always {
                    step([$class: 'JUnitResultArchiver',
                          testResults: '**/.build/sut-results.xml'])
                    archiveArtifacts artifacts: '**/.build/sut-results.xml',
                                     fingerprint: true
                }
            }
        }
    }

    post {
        unstable {
            mail to: "${params.PROJECT_GROUP_EMAIL}",
             subject: "UNSTABLE:  ${env.JOB_NAME} - ${env.BUILD_NUMBER}",
             mimeType: "text/html",
             body: "<h3 style='color:yellow;'> ${env.JOB_NAME} #${env.BUILD_NUMBER} Unstable </h3> \
                   <p> Build: <a href='${env.BUILD_URL}'>${env.JOB_NAME} #${env.BUILD_NUMBER}</a></p>"
        }
        
        failure {
            mail to: "${params.PROJECT_GROUP_EMAIL}",
             subject: "FAILURE: ${env.JOB_NAME} - ${env.BUILD_NUMBER}",
             mimeType: "text/html",
             body: "<h3 style='color:red;'> ${env.JOB_NAME} #${env.BUILD_NUMBER} Failed </h3> \
                    <p> Build: <a href='${env.BUILD_URL}'>${env.JOB_NAME} #${env.BUILD_NUMBER}</a></p>"
        }

        success {
            mail to: "${params.PROJECT_GROUP_EMAIL}",
             subject: "SUCCESS:  ${env.JOB_NAME} - ${env.BUILD_NUMBER}",
             mimeType: "text/html",
             body: "<h3 style='color:green;'> ${env.JOB_NAME} #${env.BUILD_NUMBER} Successful </h3> \
                    <p> Build: <a href='${env.BUILD_URL}'>${env.JOB_NAME} #${env.BUILD_NUMBER}</a></p>"
        }
    }
}
