node('linux')
{
    checkout scm
    
    def customImage = docker.build("smartKVM:${env.BUILD_ID}")
    def container = customImage.run()
    sh script: "docker wait ${container.id}"
    sh script: "docker logs ${container.id}"
    
    sh label: 'copying artefacts from docker', script: "docker cp ${container.id}:/root/host host"
    sh label: 'copying artefacts from docker', script: "docker cp ${container.id}:/root/usbext-build/usbext.ino.hex usbext.ino.hex"
    archiveArtifacts('host')
    archiveArtifacts('usbext.ino.hex')
}