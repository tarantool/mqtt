stage('Build'){
    packpack = new org.tarantool.packpack()

    // No mosquitto library on centos 6
    matrix = packpack.filterMatrix(
        packpack.default_matrix,
        {!(it['OS'] == 'centos' && it['DIST'] == '6')})

    node {
        checkout scm
        packpack.prepareSources()
    }
    packpack.packpackBuildMatrix('result', matrix)
}
